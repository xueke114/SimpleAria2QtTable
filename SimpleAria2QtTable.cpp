//
// Created by xueke on 2022/2/15.
//
#include <aria2/aria2.h>
#include <QLabel>
#include <QThread> //Qt5 Required
#include <QMetaType> // Qt5 Required
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QFileDialog>
#include <QToolButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QTextStream>
#include <QTableWidget>
#include <QProgressBar>
#include <QApplication>

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

class DownloadManager : public QWidget {
Q_OBJECT
public:
    DownloadManager();

private:
    void initFace();

    void initPushButton();

private slots:

    void onPbImportClicked();

    void onPbStartClicked();

    void onPbStopClicked();

    void onTbGetSaveClicked();

    void updateTable(const QPair<QVector<DownloadStatus>, int> &);


private:
    QStringList downloadLinks;
    QDir saveDir;
private:
    QPushButton *pbImportLinks = new QPushButton("导入链接");
    QPushButton *pbStart = new QPushButton("全部开始");
    QPushButton *pbPause = new QPushButton("全部暂停");
    QPushButton *pbContinue = new QPushButton("全部继续");
    QPushButton *pbStop = new QPushButton("全部停止");

    QProgressBar *progress = new QProgressBar();
    QTableWidget *tableList = new QTableWidget();

    QLineEdit *leSaveDir = new QLineEdit();
    QToolButton *tbGetSaveDir = new QToolButton();
};

void DownloadManager::initFace() {
    //pushButton 初始状态
    initPushButton();

    resize(500, 300);
    auto *lbProgress = new QLabel("总体进度：");
    auto *lbSaveTo = new QLabel("保存至：");
    tbGetSaveDir->setText("...");
    progress->setFormat("%v/%m");

    // 表头
    tableList->horizontalHeader()->setStretchLastSection(true);
    tableList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableList->setColumnCount(5);
    tableList->setHorizontalHeaderItem(0, new QTableWidgetItem("GID"));
    tableList->setHorizontalHeaderItem(1, new QTableWidgetItem("文件大小"));
    tableList->setHorizontalHeaderItem(2, new QTableWidgetItem("下载速度"));
    tableList->setHorizontalHeaderItem(3, new QTableWidgetItem("进度"));
//    tableList->setHorizontalHeaderItem(4, new QTableWidgetItem("剩余时间"));
    tableList->setHorizontalHeaderItem(4, new QTableWidgetItem("文件名"));

    auto *layout = new QGridLayout(this);
    layout->addWidget(pbImportLinks, 0, 0, 1, 2);
    layout->addWidget(pbStart, 0, 2, 1, 2);
    layout->addWidget(pbPause, 0, 4, 1, 2);
    layout->addWidget(pbContinue, 0, 6, 1, 2);
    layout->addWidget(pbStop, 0, 8, 1, 2);

    layout->addWidget(lbProgress, 1, 0, 1, 1);
    layout->addWidget(progress, 1, 1, 1, 9);

    layout->addWidget(tableList, 2, 0, 2, 10);

    layout->addWidget(lbSaveTo, 4, 0, 1, 1);
    layout->addWidget(leSaveDir, 4, 1, 1, 6);
    layout->addWidget(tbGetSaveDir, 4, 7, 1, 1);

}

DownloadManager::DownloadManager() {
    initFace();
    connect(pbImportLinks, &QPushButton::clicked, this, &DownloadManager::onPbImportClicked);
    connect(pbStart, &QPushButton::clicked, this, &DownloadManager::onPbStartClicked);
    connect(tbGetSaveDir, &QToolButton::clicked, this, &DownloadManager::onTbGetSaveClicked);
}

void DownloadManager::onPbImportClicked() {
    auto filePath = QFileDialog::getOpenFileName(this, "选择下载链接文件", "", "*.txt");
    while (filePath.isEmpty()) return;
    QFile file(filePath);
    saveDir.setPath(QFileInfo(file).canonicalPath());
    leSaveDir->setText(saveDir.path());
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        downloadLinks.clear();
        QTextStream stream(&file);
        while (!stream.atEnd())
            downloadLinks << stream.readLine().trimmed();
    }
    file.close();

    progress->setMaximum(static_cast<int>(downloadLinks.count()));
    pbStart->setEnabled(true);
}

void DownloadManager::onPbStartClicked() {
    if (leSaveDir->text().trimmed().isEmpty()) {
        QMessageBox::critical(this, "错误", "保存位置未设置");
        return;
    }
    saveDir.setPath(leSaveDir->text().trimmed());
    auto *aria2 = new Aria2Thread(downloadLinks, saveDir.path());

    connect(aria2, &Aria2Thread::resultReady, this, &DownloadManager::updateTable);
    connect(pbPause, &QPushButton::clicked, aria2, &Aria2Thread::pauseAll);
    connect(pbContinue, &QPushButton::clicked, aria2, &Aria2Thread::unpauseAll);
    connect(pbStop, &QPushButton::clicked, aria2, &Aria2Thread::shutdown);
    connect(pbStop, &QPushButton::clicked, this, &DownloadManager::onPbStopClicked);


    aria2->start();
    pbStart->setDisabled(true);
    pbPause->setEnabled(true);
    pbContinue->setEnabled(true);
    pbStop->setEnabled(true);
}

void DownloadManager::onPbStopClicked() {
    initPushButton();
    tableList->setRowCount(0);
    progress->reset();
    leSaveDir->clear();
}

void DownloadManager::onTbGetSaveClicked() {
    auto dir = QFileDialog::getExistingDirectory(this, "选择保存位置", "");
    if (dir.isEmpty())
        return;
    saveDir.setPath(dir);
    leSaveDir->setText(saveDir.path());
}

void DownloadManager::updateTable(const QPair<QVector<DownloadStatus>, int> &fromAria2) {
    tableList->setRowCount(static_cast<int>(fromAria2.first.size()));
    progress->setValue(fromAria2.second);
    int i = 0;
    for (const auto &st: fromAria2.first) {
        auto *itemGID = new QTableWidgetItem(QString::fromStdString(aria2::gidToHex(st.gid)));
        auto *itemFileSize = new QTableWidgetItem(QLocale().formattedDataSize(st.totalLength));
        auto *itemSpeed = new QTableWidgetItem(QLocale().formattedDataSize(st.downloadSpeed) + " /s");
        auto *itemFileName = new QTableWidgetItem(st.filename);

        qint64 p = st.totalLength > 0 ? (100 * st.completedLength / st.totalLength) : 0;
        auto *itemProgress = new QTableWidgetItem(QString("%1 %").arg(p));
        tableList->setItem(i, 0, itemGID);
        tableList->setItem(i, 1, itemFileSize);
        tableList->setItem(i, 2, itemSpeed);
        tableList->setItem(i, 3, itemProgress);
        tableList->setItem(i, 4, itemFileName);
        i++;
    }
}

void DownloadManager::initPushButton() {
    pbStart->setDisabled(true);
    pbContinue->setDisabled(true);
    pbPause->setDisabled(true);
    pbStop->setDisabled(true);
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
