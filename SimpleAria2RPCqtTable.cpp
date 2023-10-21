//
// Created by xueke on 2023/10/21.
//

#include <QApplication>
#include <QWidget>

class DownloadManager : public QWidget {
Q_OBJECT
};

int main(int argc, char *argv[]) {

    QApplication app(argc, argv);
    DownloadManager downloadManager;
    downloadManager.show();
    return QApplication::exec();
}

#include "SimpleAria2RPCqtTable.moc"
