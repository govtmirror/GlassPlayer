// conn_xcast.cpp
//
// Server connector for Icecast/Shoutcast streams.
//
//   (C) Copyright 2014-2016 Fred Gleason <fredg@paravelsystems.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <QByteArray>
#include <QStringList>

#include "conn_xcast.h"
#include "logging.h"

XCast::XCast(QObject *parent)
  : Connector(parent)
{
  xcast_header_active=false;
  xcast_header="";

  xcast_socket=new QTcpSocket(this);
  connect(xcast_socket,SIGNAL(connected()),this,SLOT(connectedData()));
  connect(xcast_socket,SIGNAL(readyRead()),this,SLOT(readyReadData()));
  connect(xcast_socket,SIGNAL(error(QAbstractSocket::SocketError)),
	  this,SLOT(errorData(QAbstractSocket::SocketError)));
}


XCast::~XCast()
{
  delete xcast_socket;
}


Connector::ServerType XCast::serverType() const
{
  return Connector::XCastServer;
}


void XCast::connectToHostConnector(const QString &hostname,uint16_t port)
{
  xcast_socket->connectToHost(hostname,port);
}


void XCast::disconnectFromHostConnector()
{
}


void XCast::connectedData()
{
  xcast_header_active=true;
  xcast_result_code=0;
  xcast_metadata_interval=0;
  xcast_metadata_istate=0;
  xcast_metadata_string="";
  xcast_metadata_counter=0;
  SendHeader("GET "+serverMountpoint()+" HTTP/1.1");
  SendHeader("Host: "+hostHostname()+":"+QString().sprintf("%u",hostPort()));
  SendHeader(QString().sprintf("icy-metadata: %d",streamMetadataEnabled()));
  SendHeader("Accept: */*");
  SendHeader("User-Agent: glassplayer/"+QString(VERSION));
  SendHeader("Cache-control: no-cache");
  SendHeader("Connection: close");
  SendHeader("");
}


void XCast::readyReadData()
{
  QByteArray data;
  int md_start;
  int md_len;

  while(xcast_socket->bytesAvailable()>0) {
    md_start=0;
    data=xcast_socket->read(1024);
    if(xcast_header_active) {   // Get headers
      for(int i=0;i<data.length();i++) {
	switch(0xFF&data.data()[i]) {
	case 13:
	  if(!xcast_header.isEmpty()) {
	    ProcessHeader(xcast_header);
	  }
	  break;

	case 10:
	  if(xcast_header.isEmpty()) {
	    xcast_header_active=false;
	    emit connected(true);
	    return;
	  }
	  xcast_header="";
	  break;

	default:
	  xcast_header+=data.data()[i];
	  break;
	}
      }
    }
    else {   // Scan for metadata updates
      if(xcast_metadata_counter+data.length()>xcast_metadata_interval) {
	md_start=xcast_metadata_interval-xcast_metadata_counter;
	md_len=0xFF&data[md_start]*16;
	ProcessMetadata(data.mid(md_start+1,md_len));
	xcast_metadata_counter=data.size()-(md_start+md_len+1);
	data.remove(md_start,md_len+1);
      }
      else {
	xcast_metadata_counter+=data.length();
      }
      emit dataReceived(data);
    }
  }
}


void XCast::errorData(QAbstractSocket::SocketError err)
{
}


void XCast::SendHeader(const QString &str)
{
  xcast_socket->write((str+"\r\n").toUtf8(),str.length()+2);
}


void XCast::ProcessHeader(const QString &str)
{
  QStringList f0;

  //fprintf(stderr,"%s\n",(const char *)str.toUtf8());

  if(xcast_result_code==0) {
    f0=str.split(" ",QString::SkipEmptyParts);
    if(f0.size()!=3) {
      Log(LOG_ERR,"malformed response from server ["+str+"]");
      exit(256);
    }
    xcast_result_code=f0[1].toInt();
    if((xcast_result_code<200)||(xcast_result_code>=300)) {
      Log(LOG_ERR,"server returned error ["+f0[1]+" "+f0[2]+"]");
      exit(256);
    }
  }
  else {
    f0=str.split(":");
    if(f0.size()==2) {
      QString hdr=f0[0].trimmed().toLower();
      QString value=f0[1].trimmed();
      if(hdr=="content-type") {
	setContentType(value);
      }
      if(hdr=="icy-br") {
	setAudioBitrate(value.toInt());
      }
      if(hdr=="icy-description") {
	setStreamDescription(value);
      }
      if(hdr=="icy-genre") {
	setStreamGenre(value);
      }
      if(hdr=="icy-metaint") {
	xcast_metadata_interval=value.toInt();
      }
      if(hdr=="icy-name") {
	setStreamName(value);
      }
      if(hdr=="icy-public") {
	setStreamPublic(value.toInt()!=0);
      }
      if(hdr=="icy-url") {
	setStreamUrl(value);
      }
    }
  }
}


void XCast::ProcessMetadata(const QByteArray &mdata)
{
  QStringList f0=QString(mdata).split("=");
  if(f0.size()>1) {
    if(f0[0].toLower()=="streamtitle") {
      setStreamMetadata(mdata.mid(13,mdata.lastIndexOf("';")-13));
    }
  }
}
