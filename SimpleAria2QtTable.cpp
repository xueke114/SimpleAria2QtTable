//
// Created by xueke on 2022/2/15.
//
#include <aria2/aria2.h>
#include <QThread> //Qt5 Required
#include <QMetaType> // Qt5 Required
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QApplication>

#include "ui_DownloadManager.h"

// 要从下载线程传递给UI线程的原子数据包
struct DownloadStatus {
    aria2::A2Gid gid = 0;
    int64_t totalLength = 0;
    int64_t completedLength = 0;
    int downloadSpeed = 0;
    QString filename;
};

// 下载线程
class Aria2Thread : public QThread {
Q_OBJECT

    // 将Session对象独立出来，用于暂停功能的实现
    aria2::Session *session;
    // addUrl时存储所有GID，也是为了辅助暂停功能
    QList<aria2::A2Gid> allGids = {};
    // 从allGids中提取出处于暂停状态的gids，用于恢复下载
    QVector<aria2::A2Gid> pausedGids = {};
    // allGids的size，用于进度条
    int sessionCount = 0;

    bool isPaused = false;

    bool isStopped = false;

    // 下载线程的主循环
    void run() override {
        auto start = std::chrono::steady_clock::now();
        for (;;) {
            if (1 != aria2::run(session, aria2::RUN_ONCE))
                // TODO: 应该有所表示
                break;
            auto now = std::chrono::steady_clock::now();
            auto count = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (count >= 900) {
                start = now;
                QVector<DownloadStatus> thisDownloadStatus;
                for (auto gid: aria2::getActiveDownload(session)) {
                    auto dh = aria2::getDownloadHandle(session, gid);
                    if (dh) {
                        DownloadStatus st;
                        st.gid = gid;
                        st.totalLength = dh->getTotalLength();
                        st.completedLength = dh->getCompletedLength();
                        st.downloadSpeed = dh->getDownloadSpeed();
                        if (dh->getNumFiles() > 0)
                            st.filename = QFileInfo(QString::fromStdString(dh->getFile(1).path)).fileName();
                        thisDownloadStatus.push_back(std::move(st));
                        aria2::deleteDownloadHandle(dh);
                    }
                }
                if (isPaused || isStopped)
                    continue;
                auto gs = aria2::getGlobalStat(session);
                emit resultReady({thisDownloadStatus, sessionCount - gs.numActive - gs.numWaiting});
            }
        }
        aria2::libraryDeinit();
    }

signals:

    void resultReady(QPair<QVector<DownloadStatus>, int>);


public slots:

    void pauseAll() {
        pausedGids.clear();
        for (auto gid: allGids) {
            auto dh = aria2::getDownloadHandle(session, gid);
            if (dh->getStatus() == aria2::DOWNLOAD_ACTIVE || dh->getStatus() == aria2::DOWNLOAD_WAITING) {
                aria2::pauseDownload(session, gid);
                pausedGids.push_back(gid);
            }
        }
        isPaused = true;
    }

    void unpauseAll() {
        for (auto gid: pausedGids)
            aria2::unpauseDownload(session, gid);
        isPaused = false;
    }

    void shutdown() {
        isStopped = true;
        aria2::shutdown(session);
        this->quit();
    }

public:
    Aria2Thread(const QStringList &links, const QString &saveTo) {
        aria2::libraryInit();
        aria2::SessionConfig sessionConfig;
        sessionConfig.keepRunning = true;
        aria2::KeyVals sessionOptions;
//        sessionOptions.push_back({"all-proxy", "127.0.0.1:7070"});
//        sessionOptions.push_back({"header",
//                                  "Authorization:Bearer c2FuY2hvcjpNVFF6TkRFME5UZ3lOVUJ4Y1M1amIyMD06MTY0NDcyMTI4NDplMTljMWE4MzYxNmFlYzc1YjczMzYwNDU3MmY4NDI5NWM0NDgxMTA5"});
        sessionOptions.push_back({"dir", saveTo.toStdString()});
        sessionOptions.push_back({"x", "6"});
        sessionOptions.push_back({"continue", "true"});
#ifdef _WIN32
        sessionOptions.push_back({"check-certificate", "false"});
        //Or specify the ca-certificate files, thanks to https://github.com/q3aql/aria2-static-builds#ca-certificates-on-windows-https
        //The ca-certificate.crt file can be downloaded from https://github.com/q3aql/aria2-static-builds/tree/master/certs
        //sessionOptions.push_back({"ca-certificate","ca-certificates.crt"});
#endif
        // 创建一个session
        session = aria2::sessionNew(sessionOptions, sessionConfig);
        for (const auto &link: links) {
            aria2::A2Gid gid;
            aria2::addUri(session, &gid, {link.toStdString()}, aria2::KeyVals());
            allGids.push_back(gid);
            sessionCount += 1;
        }
    }
};

namespace Ui {
    class DownloadManager;
}

class DownloadManager : public QWidget {
Q_OBJECT

public:
    explicit DownloadManager(QWidget *parent = nullptr);

    ~DownloadManager() override;

private slots:

    void onPbImportClicked();

    void onPbStartClicked();

    void onPbStopClicked();

    void onTbGetSaveClicked();

    void updateTable(const QPair<QVector<DownloadStatus>, int> &);


private:
    QStringList downloadLinks;
    Ui::DownloadManager *ui;
    QDir saveDir;
};

DownloadManager::DownloadManager(QWidget *parent) : QWidget(parent), ui(new Ui::DownloadManager) {
    ui->setupUi(this);
    connect(ui->pbImportLinks, &QPushButton::clicked, this, &DownloadManager::onPbImportClicked);
    connect(ui->pbStart, &QPushButton::clicked, this, &DownloadManager::onPbStartClicked);
    connect(ui->tbGetSaveDir, &QToolButton::clicked, this, &DownloadManager::onTbGetSaveClicked);
}

DownloadManager::~DownloadManager() {
    delete ui;
}

void DownloadManager::onPbImportClicked() {
    auto filePath = QFileDialog::getOpenFileName(this, "选择下载链接文件", "", "*.txt");
    while (filePath.isEmpty()) return;
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

    ui->progress->setMaximum(static_cast<int>(downloadLinks.count()));
    ui->pbStart->setEnabled(true);
}

void DownloadManager::onPbStartClicked() {
    if (ui->leSaveDir->text().trimmed().isEmpty()) {
        QMessageBox::critical(this, "错误", "保存位置未设置");
        return;
    }
    saveDir.setPath(ui->leSaveDir->text().trimmed());
    auto *aria2 = new Aria2Thread(downloadLinks, saveDir.path());

    connect(aria2, &Aria2Thread::resultReady, this, &DownloadManager::updateTable);
    connect(ui->pbPause, &QPushButton::clicked, aria2, &Aria2Thread::pauseAll);
    connect(ui->pbContinue, &QPushButton::clicked, aria2, &Aria2Thread::unpauseAll);
    connect(ui->pbStop, &QPushButton::clicked, aria2, &Aria2Thread::shutdown);
    connect(ui->pbStop, &QPushButton::clicked, this, &DownloadManager::onPbStopClicked);


    aria2->start();
    ui->pbStart->setDisabled(true);
    ui->pbPause->setEnabled(true);
    ui->pbContinue->setEnabled(true);
    ui->pbStop->setEnabled(true);
}

void DownloadManager::onPbStopClicked() {
    ui->pbStart->setDisabled(true);
    ui->pbContinue->setDisabled(true);
    ui->pbPause->setDisabled(true);
    ui->pbStop->setDisabled(true);
    ui->tableList->setRowCount(0);
    ui->progress->reset();
    ui->leSaveDir->clear();
}

void DownloadManager::onTbGetSaveClicked() {
    auto dir = QFileDialog::getExistingDirectory(this, "选择保存位置", "");
    if (dir.isEmpty())
        return;
    saveDir.setPath(dir);
    ui->leSaveDir->setText(saveDir.path());
}

void DownloadManager::updateTable(const QPair<QVector<DownloadStatus>, int> &fromAria2) {
    ui->tableList->setRowCount(static_cast<int>(fromAria2.first.size()));
    ui->progress->setValue(fromAria2.second);
    int i = 0;
    for (const auto &st: fromAria2.first) {
        auto *itemGID = new QTableWidgetItem(QString::fromStdString(aria2::gidToHex(st.gid)));
        auto *itemFileSize = new QTableWidgetItem(QLocale().formattedDataSize(st.totalLength));
        auto *itemSpeed = new QTableWidgetItem(QLocale().formattedDataSize(st.downloadSpeed) + " /s");
        auto *itemFileName = new QTableWidgetItem(st.filename);

        qint64 p = st.totalLength > 0 ? (100 * st.completedLength / st.totalLength) : 0;
        auto *itemProgress = new QTableWidgetItem(QString("%1 %").arg(p));
        ui->tableList->setItem(i, 0, itemGID);
        ui->tableList->setItem(i, 1, itemFileSize);
        ui->tableList->setItem(i, 2, itemSpeed);
        ui->tableList->setItem(i, 3, itemProgress);
        ui->tableList->setItem(i, 4, itemFileName);
        i++;
    }
}


int main(int argc, char *argv[]) {

#if QT_VERSION < 0x060000
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    qRegisterMetaType<QPair<QVector<DownloadStatus>, int>>("QPair<QVector<DownloadStatus>,int>");
#endif
    QApplication app(argc, argv);
    DownloadManager downloadManager;
    downloadManager.show();
    return QApplication::exec();
}

#include "SimpleAria2QtTable.moc"
