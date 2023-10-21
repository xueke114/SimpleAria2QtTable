//
// Created by xueke on 2023/10/21.
//

#include <QDir>
#include <QWidget>
#include <QApplication>
#include <QFileDialog>

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

private slots:

    void onPbImportClicked();
};

DownloadManager::DownloadManager(QWidget *parent) : QWidget(parent), ui(new Ui::DownloadManager) {
    ui->setupUi(this);
    connect(ui->pbImportLinks, &QPushButton::clicked, this, &DownloadManager::onPbImportClicked);
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
