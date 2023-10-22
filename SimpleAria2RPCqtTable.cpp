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
#include <QJsonObject>
#include <QJsonArray>
#include <utility>

#include "ui_DownloadManager.h"

namespace Ui {
    class DownloadManager;
}

class Aria2 {
public:
    explicit Aria2(const QUrl &aria2Server);

    QString addUri(QUrl downloadLink, QMap<QString, QString> options);

private:
    QUrl requestUrl;
    QJsonObject postInfo;
    QNetworkRequest req;
    QNetworkAccessManager qnam;
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply;
};

Aria2::Aria2(const QUrl &aria2Server) {
    if (aria2Server.isValid())
        req.setUrl(aria2Server);

    postInfo["id"] = "qwer";
    postInfo["jsonrpc"] = "2.0";
    postInfo["method"] = "aria2.addUri";
}

QString Aria2::addUri(QUrl downloadLink, QMap<QString, QString> options) {
    QJsonArray array_urls;
    QJsonArray array_params;
    array_urls << "https://dldir1.qq.com/weixin/Windows/WeChatSetup.exe";

    array_params << array_urls;
    postInfo["params"] = array_params;
    auto json_data = QJsonDocument(postInfo).toJson(QJsonDocument::Compact);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    qnam.post(req, json_data);
    return QString();
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
