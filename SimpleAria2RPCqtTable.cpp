//
// Created by xueke on 2023/10/21.
//

#include <QDir>
#include <QWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QNetworkAccessManager>

#include "ui_DownloadManager.h"

namespace Ui {
    class DownloadManager;
}

class DownloadManager : public QWidget {
Q_OBJECT

public:
    explicit DownloadManager(QWidget *parent = nullptr);

    ~DownloadManager() override;

private:
    QStringList downloadLinks;
    Ui::DownloadManager *ui;
    QDir saveDir;
    QNetworkAccessManager qnam;
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply;

private slots:

    void onPbImportClicked();

    void onPbStartClicked();
};

DownloadManager::DownloadManager(QWidget *parent) : QWidget(parent), ui(new Ui::DownloadManager) {
    ui->setupUi(this);
    connect(ui->pbImportLinks, &QPushButton::clicked, this, &DownloadManager::onPbImportClicked);
    connect(ui->pbStart, &QPushButton::clicked, this, &DownloadManager::onPbStartClicked);

}

void DownloadManager::onPbImportClicked() {
    auto filePath = QFileDialog::getOpenFileName(this, "选择下载链接文件", "", "*.txt");
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        saveDir.setPath(QFileInfo(file).canonicalPath());
        ui->leSaveDir->setText(saveDir.path());
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            downloadLinks.clear();
            QTextStream stream(&file);
            while (!stream.atEnd())
                downloadLinks << stream.readLine().trimmed();
        }
        file.close();
    }

    ui->progress->setMaximum(static_cast<int>(downloadLinks.count()));
    ui->pbStart->setEnabled(true);
}

void DownloadManager::onPbStartClicked() {
    if (ui->leSaveDir->text().trimmed().isEmpty()) {
        QMessageBox::critical(this, "错误", "保存位置未设置");
        return;
    }
    saveDir.setPath(ui->leSaveDir->text().trimmed());

    QNetworkRequest req(QUrl("http://127.0.0.1:6800"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    qnam.post(req, QByteArray(
            "{'jsonrpc':'2.0','id':'qwer','method':'aria2.addUri','params':[['https://dldir1.qq.com/weixin/Windows/WeChatSetup.exe']]}"));


}

DownloadManager::~DownloadManager() {
    delete ui;
}

int main(int argc, char *argv[]) {

    QApplication app(argc, argv);
    DownloadManager downloadManager;
    downloadManager.show();
    return QApplication::exec();
}

#include "SimpleAria2RPCqtTable.moc"
