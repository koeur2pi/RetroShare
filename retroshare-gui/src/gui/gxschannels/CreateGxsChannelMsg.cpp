/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2008 Robert Fernie
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

#include <QDragEnterEvent>
#include <QMessageBox>
#include <QBuffer>
#include <QMenu>
#include <QDir>
#include <QMimeData>

#include <gui/RetroShareLink.h>
#include "CreateGxsChannelMsg.h"
#include "gui/feeds/SubFileItem.h"
#include "util/misc.h"

#include <retroshare/rsfiles.h>

#include <iostream>

//#define ENABLE_GENERATE

#define CREATEMSG_CHANNELINFO 0x002

/** Constructor */
CreateGxsChannelMsg::CreateGxsChannelMsg(const RsGxsGroupId &cId)
	: QDialog (NULL, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint), mChannelId(cId) ,mCheckAttachment(true), mAutoMediaThumbNail(false)
{
	/* Invoke the Qt Designer generated object setup routine */
	setupUi(this);

	mChannelQueue = new TokenQueue(rsGxsChannels->getTokenService(), this);

	headerFrame->setHeaderImage(QPixmap(":/images/channels.png"));
	headerFrame->setHeaderText(tr("New Channel Post"));

	setAttribute ( Qt::WA_DeleteOnClose, true );

	connect(buttonBox, SIGNAL(accepted()), this, SLOT(sendMsg()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(cancelMsg()));

	connect(addFileButton, SIGNAL(clicked() ), this , SLOT(addExtraFile()));
	connect(addfilepushButton, SIGNAL(clicked() ), this , SLOT(addExtraFile()));	
	connect(addThumbnailButton, SIGNAL(clicked() ), this , SLOT(addThumbnail()));
	connect(thumbNailCb, SIGNAL(toggled(bool)), this, SLOT(allowAutoMediaThumbNail(bool)));
	connect(tabWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenu(QPoint)));
	connect(generateCheckBox, SIGNAL(toggled(bool)), generateSpinBox, SLOT(setEnabled(bool)));

	generateSpinBox->setEnabled(false);

	thumbNailCb->setVisible(false);
	thumbNailCb->setEnabled(false);
#ifdef CHANNELS_FRAME_CATCHER
	fCatcher = new framecatcher();
	thumbNailCb->setVisible(true);
	thumbNailCb->setEnabled(true);
#endif
	//buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

	setAcceptDrops(true);

	newChannelMsg();

#ifndef ENABLE_GENERATE
	generateCheckBox->hide();
	generateSpinBox->hide();
#endif
}

CreateGxsChannelMsg::~CreateGxsChannelMsg()
{
#ifdef CHANNELS_FRAME_CATCHER
	delete fCatcher;
#endif

	delete(mChannelQueue);
}

void CreateGxsChannelMsg::contextMenu(QPoint /*point*/)
{
	QList<RetroShareLink> links ;
	RSLinkClipboard::pasteLinks(links) ;

	int n_file = 0 ;

	for(QList<RetroShareLink>::const_iterator it(links.begin());it!=links.end();++it)
		if((*it).type() == RetroShareLink::TYPE_FILE)
			++n_file ;

	QMenu contextMnu(this) ;

	QAction *action ;
	if(n_file > 1)
		action = contextMnu.addAction(QIcon(":/images/pasterslink.png"), tr("Paste RetroShare Links"), this, SLOT(pasteLink()));
	else
		action = contextMnu.addAction(QIcon(":/images/pasterslink.png"), tr("Paste RetroShare Link"), this, SLOT(pasteLink()));

	action->setDisabled(n_file < 1) ;
	contextMnu.exec(QCursor::pos());
}

void CreateGxsChannelMsg::pasteLink()
{
	std::cerr << "Pasting links: " << std::endl;

	QList<RetroShareLink> links,not_have ;
	RSLinkClipboard::pasteLinks(links) ;

	for(QList<RetroShareLink>::const_iterator it(links.begin());it!=links.end();++it)
		if((*it).type() == RetroShareLink::TYPE_FILE)
		{
			// 0 - check that we actually have the file!
			//

			std::cerr << "Pasting " << (*it).toString().toStdString() << std::endl;

            FileInfo info ;
            RsFileHash hash( (*it).hash().toStdString()) ;

            if(rsFiles->alreadyHaveFile( hash,info ) )
                addAttachment(hash, (*it).name().toUtf8().constData(), (*it).size(), true, RsPeerId()) ;
			else
				not_have.push_back( *it ) ;
		}

	if(!not_have.empty())
	{
        QString msg = tr("You are about to add files you're not actually sharing. Do you still want this to happen?")+"<br><br>" ;

        for(QList<RetroShareLink>::const_iterator it(not_have.begin());it!=not_have.end();++it)
            msg += (*it).toString() + "<br>" ;

        if(QMessageBox::YesToAll == QMessageBox::question(NULL,tr("About to post un-owned files to a channel."),msg,QMessageBox::YesToAll | QMessageBox::No))
            for(QList<RetroShareLink>::const_iterator it(not_have.begin());it!=not_have.end();++it)
                addAttachment(RsFileHash((*it).hash().toStdString()), (*it).name().toUtf8().constData(), (*it).size(), false, RsPeerId()) ;
	}
}

/* Dropping */

void CreateGxsChannelMsg::dragEnterEvent(QDragEnterEvent *event)
{
	/* print out mimeType */
	std::cerr << "CreateGxsChannelMsg::dragEnterEvent() Formats";
	std::cerr << std::endl;
	QStringList formats = event->mimeData()->formats();
	QStringList::iterator it;
	for(it = formats.begin(); it != formats.end(); ++it)
	{
		std::cerr << "Format: " << (*it).toStdString();
		std::cerr << std::endl;
	}

	if (event->mimeData()->hasFormat("text/plain"))
	{
		std::cerr << "CreateGxsChannelMsg::dragEnterEvent() Accepting PlainText";
		std::cerr << std::endl;
		event->acceptProposedAction();
	}
	else if (event->mimeData()->hasUrls())
	{
		std::cerr << "CreateGxsChannelMsg::dragEnterEvent() Accepting Urls";
		std::cerr << std::endl;
		event->acceptProposedAction();
	}
	else if (event->mimeData()->hasFormat("application/x-rsfilelist"))
	{
		std::cerr << "CreateGxsChannelMsg::dragEnterEvent() accepting Application/x-qabs...";
		std::cerr << std::endl;
		event->acceptProposedAction();
	}
	else
	{
		std::cerr << "CreateGxsChannelMsg::dragEnterEvent() No PlainText/Urls";
		std::cerr << std::endl;
	}
}

void CreateGxsChannelMsg::dropEvent(QDropEvent *event)
{
	if (!(Qt::CopyAction & event->possibleActions()))
	{
		std::cerr << "CreateGxsChannelMsg::dropEvent() Rejecting uncopyable DropAction";
		std::cerr << std::endl;

		/* can't do it */
		return;
	}

	std::cerr << "CreateGxsChannelMsg::dropEvent() Formats" << std::endl;
	QStringList formats = event->mimeData()->formats();
	QStringList::iterator it;
	for(it = formats.begin(); it != formats.end(); ++it)
	{
		std::cerr << "Format: " << (*it).toStdString();
		std::cerr << std::endl;
	}

	if (event->mimeData()->hasText())
	{
		std::cerr << "CreateGxsChannelMsg::dropEvent() Plain Text:";
		std::cerr << std::endl;
		std::cerr << event->mimeData()->text().toStdString();
		std::cerr << std::endl;
	}

	if (event->mimeData()->hasUrls())
	{
		std::cerr << "CreateGxsChannelMsg::dropEvent() Urls:" << std::endl;

		QList<QUrl> urls = event->mimeData()->urls();
		QList<QUrl>::iterator uit;
		for(uit = urls.begin(); uit != urls.end(); ++uit)
		{
			QString localpath = uit->toLocalFile();
			std::cerr << "Whole URL: " << uit->toString().toStdString() << std::endl;
			std::cerr << "or As Local File: " << localpath.toStdString() << std::endl;

			if (localpath.isEmpty() == false)
			{
				// Check that the file does exist and is not a directory
				QDir dir(localpath);
				if (dir.exists()) {
					std::cerr << "CreateGxsChannelMsg::dropEvent() directory not accepted."<< std::endl;
					QMessageBox mb(tr("Drop file error."), tr("Directory can't be dropped, only files are accepted."),QMessageBox::Information,QMessageBox::Ok,0,0,this);
					mb.exec();
				} else if (QFile::exists(localpath)) {
					addAttachment(localpath.toUtf8().constData());
				} else {
					std::cerr << "CreateGxsChannelMsg::dropEvent() file does not exists."<< std::endl;
					QMessageBox mb(tr("Drop file error."), tr("File not found or file name not accepted."),QMessageBox::Information,QMessageBox::Ok,0,0,this);
					mb.exec();
				}
			}
		}
	}
	else if (event->mimeData()->hasFormat("application/x-rsfilelist"))
	{
		std::cerr << "CreateGxsChannelMsg::dropEvent() Application/x-rsfilelist";
		std::cerr << std::endl;

		QByteArray data = event->mimeData()->data("application/x-rsfilelist");
		std::cerr << "Data Len:" << data.length();
		std::cerr << std::endl;
		std::cerr << "Data is:" << data.data();
		std::cerr << std::endl;

		std::string newattachments(data.data());
		parseRsFileListAttachments(newattachments);
	}

	event->setDropAction(Qt::CopyAction);
	event->accept();
}

void CreateGxsChannelMsg::parseRsFileListAttachments(const std::string &attachList)
{
	/* split into lines */
	QString input = QString::fromStdString(attachList);

	QStringList attachItems = input.split("\n");
	QStringList::iterator it;
	QStringList::iterator it2;

	for(it = attachItems.begin(); it != attachItems.end(); ++it)
	{
		std::cerr << "CreateGxsChannelMsg::parseRsFileListAttachments() Entry: ";

		QStringList parts = (*it).split("/");

		bool ok = false;
		quint64     qsize = 0;

		std::string fname;
        RsFileHash hash;
		uint64_t    size = 0;
		RsPeerId source;

		int i = 0;
		for(it2 = parts.begin(); it2 != parts.end(); ++it2, ++i)
		{
			std::cerr << "\"" << it2->toStdString() << "\" ";
			switch(i)
			{
				case 0:
					fname = it2->toStdString();
					break;
				case 1:
                    hash = RsFileHash(it2->toStdString());
					break;
				case 2:
					qsize = it2->toULongLong(&ok, 10);
					size = qsize;
					break;
				case 3:
					source = RsPeerId(it2->toStdString());
					break;
			}
		}

		std::cerr << std::endl;

		std::cerr << "\tfname: " << fname << std::endl;
		std::cerr << "\thash: " << hash << std::endl;
		std::cerr << "\tsize: " << size << std::endl;
		std::cerr << "\tsource: " << source << std::endl;

        /* basic error checking */
        if (ok && !hash.isNull())
		{
			std::cerr << "Item Ok" << std::endl;
			addAttachment(hash, fname, size, source.isNull(), source);
		}
		else
		{
            std::cerr << "Error Decode: Hash is not a hash: " << hash << std::endl;
		}
	}
}


void CreateGxsChannelMsg::addAttachment(const RsFileHash &hash, const std::string &fname, uint64_t size, bool local, const RsPeerId &srcId)
{
	/* add a SubFileItem to the attachment section */
	std::cerr << "CreateGxsChannelMsg::addAttachment()";
	std::cerr << std::endl;

	/* add widget in for new destination */

	uint32_t flags = SFI_TYPE_CHANNEL;
	if (local)
	{
		flags |= SFI_STATE_LOCAL;
	}
	else
	{
		flags |= SFI_STATE_REMOTE;
	}

	SubFileItem *file = new SubFileItem(hash, fname, "", size, flags, srcId); // destroyed when fileFrame (this subfileitem) is destroyed

	mAttachments.push_back(file);
	QLayout *layout = fileFrame->layout();
	layout->addWidget(file);

	if (mCheckAttachment)
	{
		checkAttachmentReady();
	}

	return;
}

void CreateGxsChannelMsg::addExtraFile()
{
	/* add a SubFileItem to the attachment section */
	std::cerr << "CreateGxsChannelMsg::addExtraFile() opening file dialog";
	std::cerr << std::endl;

	QStringList files;
	if (misc::getOpenFileNames(this, RshareSettings::LASTDIR_EXTRAFILE, tr("Add Extra File"), "", files)) {
		for (QStringList::iterator fileIt = files.begin(); fileIt != files.end(); ++fileIt) {
			addAttachment((*fileIt).toUtf8().constData());
		}
	}
}

void CreateGxsChannelMsg::addAttachment(const std::string &path)
{
	/* add a SubFileItem to the attachment section */
	std::cerr << "CreateGxsChannelMsg::addAttachment()";
	std::cerr << std::endl;

	if(mAutoMediaThumbNail)
	setThumbNail(path, 2000);

	/* add widget in for new destination */
	uint32_t flags =  SFI_TYPE_CHANNEL | SFI_STATE_EXTRA | SFI_FLAG_CREATE;

	// check attachment if hash exists already
	std::list<SubFileItem* >::iterator  it;

	for(it= mAttachments.begin(); it != mAttachments.end(); ++it){

		if((*it)->FilePath() == path){
			QMessageBox::warning(this, tr("RetroShare"), tr("File already Added and Hashed"), QMessageBox::Ok, QMessageBox::Ok);

			return;
		}
	}

	FileInfo fInfo;
	std::string filename;
	uint64_t size = 0;
    RsFileHash hash ;

	rsGxsChannels->ExtraFileHash(path, filename);

    // Only path and filename are valid.
    // Destroyed when fileFrame (this subfileitem) is destroyed
    // Hash will be retrieved later when the file is finished hashing.
    //
	//SubFileItem *file = new SubFileItem(hash, filename, path, size, flags, mChannelId); 
	SubFileItem *file = new SubFileItem(hash, filename, path, size, flags, RsPeerId()); 

	mAttachments.push_back(file);
	QLayout *layout = fileFrame->layout();
	layout->addWidget(file);

	if (mCheckAttachment)
	{
		checkAttachmentReady();
	}

	return;
}

bool CreateGxsChannelMsg::setThumbNail(const std::string& path, int frame){

#ifdef CHANNELS_FRAME_CATCHER
	unsigned char* imageBuffer = NULL;
	int width = 0, height = 0, errCode = 0;
	int length;
	std::string errString;

	if(1 !=  (errCode = fCatcher->open(path))){
		fCatcher->getError(errCode, errString);
		std::cerr << errString << std::endl;
		return false;
	}

	length = fCatcher->getLength();

	// make sure frame chosen is at lease a quarter length of video length if not choose quarter length
	if(frame < (int) (0.25 * length))
		frame = 0.25 * length;

	if(1 != (errCode  = fCatcher->getRGBImage(frame, imageBuffer, width, height))){
		fCatcher->getError(errCode, errString);
		std::cerr << errString << std::endl;
		return false;
	}

	if(imageBuffer == NULL)
		return false;

	QImage tNail(imageBuffer, width, height, QImage::Format_RGB32);
	QByteArray ba;
	QBuffer buffer(&ba);
	buffer.open(QIODevice::WriteOnly);
	tNail.save(&buffer, "PNG");
	QPixmap img;
	img.loadFromData(ba, "PNG");
	img = img.scaled(thumbnail_label->width(), thumbnail_label->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	thumbnail_label->setPixmap(img);

	delete[] imageBuffer;
#else
	Q_UNUSED(path);
	Q_UNUSED(frame);
#endif

	return true;
}

void CreateGxsChannelMsg::allowAutoMediaThumbNail(bool allowThumbNail)
{
	mAutoMediaThumbNail = allowThumbNail;
}

void CreateGxsChannelMsg::checkAttachmentReady()
{
	std::list<SubFileItem *>::iterator fit;

	mCheckAttachment = false;

	for(fit = mAttachments.begin(); fit != mAttachments.end(); ++fit)
	{
		if (!(*fit)->isHidden())
		{
			if (!(*fit)->ready())
			{
				/* ensure file is hashed or file will be hashed, thus
				 * recognized by librs but not correctly by gui (can't
				 * formally remove it)
				 */
				buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
				buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
				break;
			}
		}
	}

	if (fit == mAttachments.end())
	{
		buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
		buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
	}

	/* repeat... */
	int msec_rate = 1000;
	QTimer::singleShot( msec_rate, this, SLOT(checkAttachmentReady(void)));
}

void CreateGxsChannelMsg::cancelMsg()
{
	std::cerr << "CreateGxsChannelMsg::cancelMsg() :"
			  << "Deleting EXTRA attachments" << std::endl;

	std::cerr << std::endl;

	std::list<SubFileItem* >::const_iterator it;

	for(it = mAttachments.begin(); it != mAttachments.end(); ++it)
		rsGxsChannels->ExtraFileRemove((*it)->FileHash());

	reject();
}

void CreateGxsChannelMsg::newChannelMsg()
{
	if (!rsGxsChannels)
		return;

	mChannelMetaLoaded = false;

	/* request Data */
	{
		RsTokReqOptions opts;
		opts.mReqType = GXS_REQUEST_TYPE_GROUP_META;

        std::list<RsGxsGroupId> groupIds;
		groupIds.push_back(mChannelId);

		std::cerr << "CreateGxsChannelMsg::newChannelMsg() Req Group Summary(" << mChannelId << ")";
		std::cerr << std::endl;

		uint32_t token;
		mChannelQueue->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_SUMMARY, opts, groupIds, CREATEMSG_CHANNELINFO);
	}
}

void CreateGxsChannelMsg::saveChannelInfo(const RsGroupMetaData &meta)
{
	mChannelMeta = meta;
	mChannelMetaLoaded = true;

	channelName->setText(QString::fromUtf8(mChannelMeta.mGroupName.c_str()));
	subjectEdit->setFocus();
}

void CreateGxsChannelMsg::sendMsg()
{
	std::cerr << "CreateGxsChannelMsg::sendMsg()";
	std::cerr << std::endl;

	/* construct message bits */
	std::string subject = std::string(misc::removeNewLine(subjectEdit->text()).toUtf8());
	std::string msg     = std::string(msgEdit->toPlainText().toUtf8());

	std::list<RsGxsFile> files;

	std::list<SubFileItem *>::iterator fit;

	for(fit = mAttachments.begin(); fit != mAttachments.end(); ++fit)
	{
		if (!(*fit)->isHidden())
		{
			RsGxsFile fi;
			fi.mHash = (*fit)->FileHash();
			fi.mName = (*fit)->FileName();
			fi.mSize = (*fit)->FileSize();

			files.push_back(fi);

			/* commence downloads - if we don't have the file */

			if (!(*fit)->done())
			{
				if ((*fit)->ready())
				{
					(*fit)->download();
				}
			// Skips unhashed files.
			}
		}
	}

	sendMessage(subject, msg, files);
}

void CreateGxsChannelMsg::sendMessage(const std::string &subject, const std::string &msg, const std::list<RsGxsFile> &files)
{
	if(subject.empty())
	{	/* error message */
		QMessageBox::warning(this, tr("RetroShare"), tr("Please add a Subject"), QMessageBox::Ok, QMessageBox::Ok);

		return; //Don't add  an empty Subject!!
	}
	else
	/* rsGxsChannels */
	if (rsGxsChannels)
	{
		RsGxsChannelPost post;

		post.mMeta.mGroupId = mChannelId;
		post.mMeta.mParentId.clear() ;
		post.mMeta.mThreadId.clear() ;
		post.mMeta.mMsgId.clear() ;

		post.mMeta.mMsgName = subject;
		post.mMsg = msg;
		post.mFiles = files;

		QByteArray ba;
		QBuffer buffer(&ba);

		if(!picture.isNull())
		{
			// send chan image

			buffer.open(QIODevice::WriteOnly);
			picture.save(&buffer, "PNG"); // writes image into ba in PNG format
			post.mThumbnail.copy((uint8_t *) ba.data(), ba.size());
		}

		int generateCount = 0;

#ifdef ENABLE_GENERATE
		if (generateCheckBox->isChecked()) {
			generateCount = generateSpinBox->value();
			if (QMessageBox::question(this, tr("Generate mass data"), tr("Do you really want to generate %1 messages ?").arg(generateCount), QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::No) {
				return;
			}
		}
#endif

		uint32_t token;
		if (generateCount) {
#ifdef ENABLE_GENERATE
			for (int count = 0; count < generateCount; ++count) {
				RsGxsChannelPost generatePost = post;
				generatePost.mMeta.mMsgName = QString("%1 %2").arg(QString::fromUtf8(post.mMeta.mMsgName.c_str())).arg(count + 1, 3, 10, QChar('0')).toUtf8().constData();

				rsGxsChannels->createPost(token, generatePost);
			}
#endif
		} else {
			rsGxsChannels->createPost(token, post);
		}
	}

	accept();
}

void CreateGxsChannelMsg::addThumbnail()
{
	QPixmap img = misc::getOpenThumbnailedPicture(this, tr("Load thumbnail picture"), 156, 107);

	if (img.isNull())
		return;

	picture = img;

	// to show the selected
	thumbnail_label->setPixmap(picture);
}

void CreateGxsChannelMsg::loadChannelInfo(const uint32_t &token)
{
	std::cerr << "CreateGxsChannelMsg::loadChannelInfo()";
	std::cerr << std::endl;

	std::list<RsGroupMetaData> groupInfo;
	rsGxsChannels->getGroupSummary(token, groupInfo);

	if (groupInfo.size() == 1)
	{
		RsGroupMetaData fi = groupInfo.front();
		saveChannelInfo(fi);
	}
	else
	{
		std::cerr << "CreateGxsChannelMsg::loadForumInfo() ERROR INVALID Number of Forums";
		std::cerr << std::endl;
	}
}

void CreateGxsChannelMsg::loadRequest(const TokenQueue *queue, const TokenRequest &req)
{
	std::cerr << "CreateGxsChannelMsg::loadRequest() UserType: " << req.mUserType;
	std::cerr << std::endl;

	if (queue == mChannelQueue)
	{
		/* now switch on req */
		switch(req.mUserType)
		{
			case CREATEMSG_CHANNELINFO:
				loadChannelInfo(req.mToken);
				break;
			default:
				std::cerr << "CreateGxsChannelMsg::loadRequest() UNKNOWN UserType ";
				std::cerr << std::endl;
		}
	}
}
