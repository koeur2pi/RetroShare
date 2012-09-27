#ifndef PHOTOSHARE_H
#define PHOTOSHARE_H

#include <QWidget>
#include <map>
#include "ui_PhotoShare.h"

#include "retroshare/rsphotoV2.h"
#include "retroshare-gui/mainpage.h"

#include "AlbumCreateDialog.h"
#include "AlbumDialog.h"
#include "PhotoDialog.h"

#include "AlbumItem.h"
#include "PhotoItem.h"
#include "PhotoSlideShow.h"

#include "util/TokenQueueV2.h"
#include "PhotoShareItemHolder.h"

namespace Ui {
    class PhotoShare;
}

class PhotoShare : public MainPage, public TokenResponseV2, public PhotoShareItemHolder
{
  Q_OBJECT

public:
        PhotoShare(QWidget *parent = 0);

        void notifySelection(PhotoShareItem* selection);

private slots:

        void checkUpdate();
        void createAlbum();
        void OpenAlbumDialog();
        void OpenPhotoDialog();
        void OpenSlideShow();
        void SetAlbumDialogClosed();
        void SetPhotoDialogClosed();
        void updateAlbums();
        void subscribeToAlbum();
private:

        /* Request Response Functions for loading data */
        void requestAlbumList(std::list<std::string>& ids);
        void requestAlbumData(std::list<RsGxsGroupId> &ids);

        /*!
         * request data for all groups
         */
        void requestAlbumData();
        void requestPhotoList(GxsMsgReq &albumIds);
        void requestPhotoList(const std::string &albumId);
        void requestPhotoData(GxsMsgReq &photoIds);

        void loadAlbumList(const uint32_t &token);
        bool loadAlbumData(const uint32_t &token);
        void loadPhotoList(const uint32_t &token);
        void loadPhotoData(const uint32_t &token);

        void loadRequest(const TokenQueueV2 *queue, const TokenRequestV2 &req);

        void acknowledgeGroup(const uint32_t &token);
        void acknowledgeMessage(const uint32_t &token);

        /* Grunt work of setting up the GUI */

        void addAlbum(const RsPhotoAlbum &album);
        void addPhoto(const RsPhotoPhoto &photo);

        void clearAlbums();
        void clearPhotos();
        void deleteAlbums();
        void updatePhotos();

private:


        AlbumItem* mAlbumSelected;
        PhotoItem* mPhotoSelected;
        PhotoSlideShow* mSlideShow;
        AlbumDialog* mAlbumDialog;
        PhotoDialog* mPhotoDialog;

        TokenQueueV2 *mPhotoQueue;

        /* UI - from Designer */
        Ui::PhotoShare ui;

        QSet<AlbumItem*> mAlbumItems;
        QMap<RsGxsGroupId, QSet<PhotoItem*> > mPhotoItems;

};

#endif // PHOTOSHARE_H