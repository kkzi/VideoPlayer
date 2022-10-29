#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QFileDialog>
#include <QThread>
#include <QDesktopWidget>
#include <QSizePolicy>
#include <QElapsedTimer>
#include <QTimer>
#include <QSettings>
#include <QMutex>
#include <QMimeData>
#include <QActionGroup>

//#include "decode_thread.h"
#include "video_decode_thread.h"
#include "audio_decode_thread.h"
#include "subtitle_decode_thread.h"
#include "audio_play_thread.h"
#include "video_play_thread.h"
#include "read_thread.h"
#include "start_play_thread.h"
#include "video_state.h"
#include "play_control_window.h"
#include "app_settings.h"
#include "video_label.h"
#include "player_skin.h"
#include "audio_effect_gl.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget* parent = nullptr);
	~MainWindow();

public:
	void start_to_play(const QString& file);
	bool start_play();
	bool is_playing();
	void stop_play();
	void pause_play();
	float volume_settings(bool set = true, float vol = 0);
	AudioPlayThread* get_audio_play_thread() { return m_pAudioPlayThread.get(); }
	VideoPlayThread* get_video_play_thread() { return m_pVideoPlayThread.get(); }
	VideoStateData* get_video_state_data() { return m_pVideoState.get(); }
	void play_mute(bool mute);
	void play_seek();
	void play_seek_pre();
	void play_seek_next();
	void set_volume(int volume);
	void set_play_speed();
	void show_msg_dlg(const QString& message = "", const QString& windowTitle = "Warning", const QString& styleSheet = "");

private:
	void resizeEvent(QResizeEvent* event) override;
	void moveEvent(QMoveEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	bool eventFilter(QObject* obj, QEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;

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
	void on_actionLoop_Play_triggered();
	void on_actionMedia_Info_triggered();
	void on_actionKeyboard_Usage_triggered();
	void on_actionAudio_visualize_triggered();

public slots:
	void image_ready(const QImage&);
	void subtitle_ready(const QString&);
	void audio_data(AudioData data);
	void open_recentFile();
	void clear_recentfiles();
	void read_packet_stopped();
	void decode_video_stopped();
	void decode_audio_stopped();
	void decode_subtitle_stopped();
	void audio_play_stopped();
	void video_play_stopped();
	void update_play_time();
	void play_started(bool ret = true);

signals:
	void stop_audio_play_thread();
	void stop_video_play_thread();
	void stop_decode_thread();
	void stop_read_packet_thread();
	void wait_stop_audio_play_thread();
	void wait_stop_video_play_thread();

private:
	void update_image(const QImage&);
	void print_decodeContext(const AVCodecContext* pVideo, bool bVideo = true);
	void about_media_info();
	void image_cv(QImage&);
	void resize_window(int width = 800, int height = 480);
	void center_window(QRect screen_rec);
	void show_fullscreen(bool bFullscreen = true);
	bool label_fullscreen();
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
	void print_size() const;
	void keep_aspect_ratio(bool bWidth = true);
	void create_style_menu();
	video_label* get_video_label() const;
	QObject* get_object(const QString name) const;
	void create_play_control();
	void update_play_control();
	PlayControlWnd* get_play_control() const;
	void set_volume_updown(bool bUp = true, float unit = 0.2);
	void create_recentfiles_menu();
	void set_current_file(const QString& fileName);
	void remove_recentfiles(const QString& fileName);
	void update_recentfile_actions();
	QString stripped_name(const QString& fullFileName) const;
	void save_settings();
	void read_settings();
	QString get_selected_style() const;
	void set_style_action(const QString& style);
	void clear_subtitle_str();
	void create_cv_action_group();
	void play_speed_adjust(bool up = true);
	void create_video_label();
	void update_video_label();
	void create_audio_effect();
	void start_send_data(bool bSend = true);
	void show_audio_effect(bool bShow = true);
	void play_control_key(Qt::Key key);
	void set_default_bkground();
	bool create_video_state(const char* filename, QThread* pThread);
	void delete_video_state();
	bool create_read_thread(); //read packet thread
	bool create_decode_video_thread(); //decode thread
	bool create_decode_audio_thread(); //decode audio thread
	bool create_decode_subtitle_thread(); //decode subtitle thread
	bool create_video_play_thread(); //video play thread
	bool create_audio_play_thread(); //audio play thread	
	bool start_play_thread(); //this thread will init audio device to avoid frezzing ui
	void all_thread_start();
	void video_seek_inc(double incr);
	void video_seek(double pos = 0, double incr = 0);

private:
	std::unique_ptr<Ui::MainWindow> ui;

	std::unique_ptr<ReadThread> m_pPacketReadThread; //read packets thread
	std::unique_ptr<VideoDecodeThread> m_pDecodeVideoThread; //decode video thread
	std::unique_ptr<AudioDecodeThread> m_pDecodeAudioThread; //decode audio thread
	std::unique_ptr<SubtitleDecodeThread> m_pDecodeSubtitleThread; //decode subtitle thread
	std::unique_ptr<AudioPlayThread> m_pAudioPlayThread; //audio play thread
	std::unique_ptr<VideoPlayThread> m_pVideoPlayThread; //video play thread
	std::unique_ptr<VideoStateData> m_pVideoState;	//for sync packets
	std::unique_ptr<StartPlayThread> m_pBeforePlayThread; //for time-consuming operations before play

	QString m_videoFile;
	QTimer m_timer; //mouse moving checker timer
	AppSettings m_settings;
	PlayerSkin m_skin;
	QString m_subtitle;

	std::unique_ptr<video_label> m_video_label;
	std::unique_ptr<PlayControlWnd> m_play_control_wnd;
	std::unique_ptr<AudioEffectGL> m_audio_effect_wnd;
private:
	enum {
		MaxRecentFiles = 20,  // maximum recent play files
		MaxSkinStlyes = 20,   // maximum sytyles
	};
	std::unique_ptr<QAction> m_recentFileActs[MaxRecentFiles];
	std::unique_ptr<QAction> m_recentClear;
	std::unique_ptr<QActionGroup> m_styleActsGroup; //style menus group
	std::unique_ptr<QAction> m_styleActions[MaxSkinStlyes];
	std::unique_ptr<QActionGroup> m_CvActsGroup; //cv menus group
};
#endif // MAINWINDOW_H
