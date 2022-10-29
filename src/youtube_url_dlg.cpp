// ***********************************************************/
// youtube_url_dlg.cpp
//
//      Copy Right @ Steven Huang. All rights reserved.
//
// Youtube Url open dialog
// ***********************************************************/
#include <QMessageBox>
#include "youtube_url_dlg.h"
#include "ui_youtube_url_dlg.h"
#include "mainwindow.h"

YoutubeUrlDlg::YoutubeUrlDlg(QWidget* parent)
	: QDialog(parent)
	, ui(std::make_unique<Ui::YoutubeUrlDlg>())
	, m_youtubeUrl("")
{
	ui->setupUi(this);
	setLayout(ui->gridLayout);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	// QObject::connect(ui->btn_Ok, SIGNAL(clicked()), this, SLOT(accept()));
	QObject::connect(ui->btn_Cancel, SIGNAL(clicked()), this, SLOT(reject()));
}

YoutubeUrlDlg::~YoutubeUrlDlg()
{
}

void YoutubeUrlDlg::on_btn_Ok_clicked()
{
	QString url = ui->lineEdit->text();
	if (!url.isEmpty()) {
		qDebug("Youtube Url:%s", qUtf8Printable(url));
		QString res = parse_youtubeUrl(url);
		if (!res.isEmpty()) {
			m_youtubeUrl = res;
			accept();
			return;
		}
	}

	qDebug("Invalid youtube url:%s", qUtf8Printable(url));

	MainWindow* pMain = (MainWindow*)parentWidget();
	if (pMain) {
		QString str = QString("Please input a valid youtube url. ");
		pMain->show_msg_dlg(str);
	}
}

QString YoutubeUrlDlg::parse_youtubeUrl(const QString& url)
{
	QProcess process;
	QString home = QDir::currentPath();
	QString exec = home + "/tools/youtube-dl.exe";

	QStringList params;
	params << "-f" << "best" << "-g" << url;
	process.start(exec, params);

	process.waitForFinished(); // sets current thread to sleep and waits for pingProcess end

	QProcess::ExitStatus Status = process.exitStatus();
	if (Status == QProcess::ExitStatus::NormalExit) {
		QString output(process.readAllStandardOutput());
		qDebug("output:%s", qUtf8Printable(output));
		return output;
	}
	return QString("");
}