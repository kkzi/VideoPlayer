#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <qevent.h>
#include <QFileDialog>
#include <QThread>
#include <QDesktopWidget>
#include <QSizePolicy>
#include <QElapsedTimer>
#include <QTimer>

//#include "decode_thread.h"
#include "video_decode_thread.h"
#include "audio_decode_thread.h"
#include "audio_play_thread.h"
#include "video_play_thread.h"
#include "read_thread.h"
#include "video_state.h"
#include "play_control_window.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget* parent = nullptr);
	~MainWindow();

private:
	Ui::MainWindow* ui;
	QString m_videoFile;

	ReadThread* m_pPacketReadThread; //read packets thread
	VideoDecodeThread* m_pDecodeVideoThread; //decode video thread
	AudioDecodeThread* m_pDecodeAudioThread; //decode audio thread
	//DecodeThread* m_pDecodeAudioThread; //decode audio thread

	QTimer m_timer; //mouse moving timer
	bool m_bGrayscale;
public:
	VideoStateData* m_pVideoState;	//for sync packets
	AudioPlayThread* m_pAudioPlayThread; //audio play thread
	VideoPlayThread* m_pVideoPlayThread; //video play thread
private:
	void resizeEvent(QResizeEvent* event) override;
	void moveEvent(QMoveEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
	void on_actionOpen_triggered();
	void on_actionQuit_triggered();
	void on_actionHelp_triggered();
	void on_actionAbout_triggered();
	void on_actionStop_triggered();
	void on_actionHide_Status_triggered();
	void on_actionFullscreen_triggered();
	void on_actionHide_Play_Ctronl_triggered();
	void on_actionYoutube_triggered();
	void on_actionAspect_Ratio_triggered();
	void on_actionSystemStyle();
	void on_actionCustomStyle();
	void on_actionGrayscale_triggered();
public slots:
	void image_ready(const QImage&);
	void update_image(const QImage&);
	void print_decodeContext(const AVCodecContext* pVideo, bool bVideo = true);
	void decode_video_stopped();
	void decode_audio_stopped();
	void audio_play_stopped();
	void video_play_stopped();
	void read_packet_stopped();
	void update_play_time();
	void play_started(bool ret);
	//void play_seek(int value);
	void play_seek();
	void play_seek_pre();
	void play_seek_next();
	void play_mute(bool mute);
	void set_volume(int volume);
signals:
	void stop_audio_play_thread();
	void stop_video_play_thread();
	void stop_decode_thread();
	void stop_read_packet_thread();
	void wait_stop_audio_play_thread();
	void wait_stop_video_play_thread();

private:
	void resize_window(int width = 800, int height = 480);
	void center_window(QRect screen_rec);
	void show_fullscreen(bool bFullscreen = true);
	void hide_statusbar(bool bHide = true);
	void hide_menubar(bool bHide = true);
	void check_hide_menubar(QMouseEvent* mouseEvent);
	void check_hide_play_control();
	void auto_hide_play_control(bool bHide = true);
	void displayStatusMessage(const QString& message);
	void hide_play_control(bool bHide = true);
	void set_paly_control_wnd(bool set = true);
	void update_paly_control_volume();
	void update_paly_control_status();
	void update_paly_control_muted();
	void print_size();
	void keep_aspect_ratio(bool bWidth = true);
	void create_style_menu();
	QLabel* get_video_label();
	QObject* get_object(const QString name);
	void create_play_control();
	void update_play_control();
	void set_volume_updown(bool bUp = true, float unit = 0.2);
private:
	void play_control_key(Qt::Key key);
	void set_default_bkground();

	bool create_video_state(const char* filename, QThread* pThread);
	void delete_video_state();

	bool create_read_thread(); //read packet thread
	bool create_decode_video_thread(); //decode thread
	bool create_decode_audio_thread(); //decode audio thread
	bool create_video_play_thread(); //video play thread
	bool create_audio_play_thread(); //audio play thread
	void all_thread_start();

	void video_seek_inc(double incr);
	void video_seek(double pos = 0, double incr = 0);
public:
	bool start_play();
	void stop_play();
	void pause_play();
};
#endif // MAINWINDOW_H
