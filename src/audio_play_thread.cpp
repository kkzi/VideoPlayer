#include "audio_play_thread.h"


AudioPlayThread::AudioPlayThread(QObject* parent, VideoState* pState)
	: QThread(parent)
	, m_pDevice(NULL)
	, m_pOutput(NULL)
	, m_pState(pState)
	, m_bExitThread(false)
{
	memset(&m_audioResample, 0, sizeof(Audio_Resample));
}

AudioPlayThread::~AudioPlayThread()
{
	stop_thread();
	stop_device();
	final_resample_param();
}

void AudioPlayThread::print_device()
{
	QAudioDeviceInfo deviceInfo = QAudioDeviceInfo::defaultOutputDevice();
	auto deviceInfos = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
	for (const QAudioDeviceInfo& deviceInfo : deviceInfos)
		qDebug() << "Input device name: " << deviceInfo.deviceName();

	deviceInfos = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
	for (const QAudioDeviceInfo& deviceInfo : deviceInfos)
		qDebug() << "Output device name: " << deviceInfo.deviceName();

	auto edians = deviceInfo.supportedByteOrders();
	for (const QAudioFormat::Endian& endian : edians)
		qDebug() << "Endian: " << endian;
	auto sampleTypes = deviceInfo.supportedSampleTypes();
	for (const QAudioFormat::SampleType& sampleType : sampleTypes)
		qDebug() << "sampleType: " << sampleType;

	auto codecs = deviceInfo.supportedCodecs();
	for (const QString& codec : codecs)
		qDebug() << "codec: " << codec;

	auto sampleRates = deviceInfo.supportedSampleRates();
	for (const int& sampleRate : sampleRates)
		qDebug() << "sampleRate: " << sampleRate;

	auto ChannelCounts = deviceInfo.supportedChannelCounts();
	for (const int& channelCount : ChannelCounts)
		qDebug() << "channelCount: " << channelCount;

	auto sampleSizes = deviceInfo.supportedSampleSizes();
	for (const int& sampleSize : sampleSizes)
		qDebug() << "sampleSize: " << sampleSize;
}

bool AudioPlayThread::init_device(int sample_rate, int channel, AVSampleFormat sample_fmt)
{
	QAudioDeviceInfo deviceInfo = QAudioDeviceInfo::defaultOutputDevice();
	m_pDevice = &deviceInfo;

	QAudioFormat format;
	// Set up the format, eg.
	format.setSampleRate(sample_rate);
	format.setChannelCount(channel);
	format.setSampleSize(8 * av_get_bytes_per_sample(sample_fmt));
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::SignedInt);

	// qDebug("sample size=%d\n", 8 * av_get_bytes_per_sample(sample_fmt));
	if (!m_pDevice->isFormatSupported(format)) {
		qWarning() << "Raw audio format not supported by backend, cannot play audio.";
		return false;
	}

	m_pOutput = new QAudioOutput(*m_pDevice, format);
	set_device_volume(0.8);

	m_audioDevice = m_pOutput->start();
	return true;
}

float AudioPlayThread::get_device_volume()
{
	if (m_pOutput) {
		return m_pOutput->volume();
	}
	return 0;
}

void AudioPlayThread::set_device_volume(float volume)
{
	if (m_pOutput) {
		m_pOutput->setVolume(volume);
	}
}

void AudioPlayThread::stop_device()
{
	if (m_pOutput)
	{
		m_pOutput->stop();
		delete m_pOutput;
		m_pOutput = NULL;
	}
}

void AudioPlayThread::play_file(const QString& file)
{
	/*play pcm file directly*/
	QFile audioFile;
	audioFile.setFileName(file);
	audioFile.open(QIODevice::ReadOnly);
	m_pOutput->start(&audioFile);
}

void AudioPlayThread::play_buf(const uint8_t* buf, int datasize)
{
	if (m_audioDevice)
	{
		uint8_t* data = (uint8_t*)buf;
		while (datasize > 0) {
			qint64 len = m_audioDevice->write((const char*)data, datasize);
			if (len < 0)
				break;
			if (len > 0) {
				data = data + len;
				datasize -= len;
			}
			// qDebug("play buf:reslen:%d, write len:%d", len, datasize);
		}
	}

	av_free((void*)buf);
}

void AudioPlayThread::run()
{
	assert(m_pState);
	VideoState* is = m_pState;
	int audio_size;

	for (;;) {
		if (m_bExitThread)
			break;

		if (is->abort_request)
			break;

		if (is->paused)	{
			msleep(10);
			continue;
		}

		audio_size = audio_decode_frame(is);
		if (audio_size < 0)
			break;
	}

	qDebug("-------- Audio play thread exit.");
}

int AudioPlayThread::audio_decode_frame(VideoState* is)
{
	int data_size;
	double audio_clock0;
	Frame* af;

	do {
#if defined(_WIN32)
		while (frame_queue_nb_remaining(&is->sampq) == 0) {
			av_usleep(1000);
			//return -1;
		}
#endif
		if (!(af = frame_queue_peek_readable(&is->sampq)))
			return -1;
		frame_queue_next(&is->sampq);
	} while (af->serial != is->audioq.serial);


	struct SwrContext* swrCtx = m_audioResample.swrCtx;
	data_size = av_samples_get_buffer_size(nullptr, af->frame->channels, af->frame->nb_samples,
		AV_SAMPLE_FMT_S16, 0);  //AVSampleFormat(af->frame->format)
	uint8_t* buffer_audio = (uint8_t*)av_malloc(data_size * sizeof(uint8_t));
	swr_convert(swrCtx, &buffer_audio, af->frame->nb_samples, (const uint8_t**)(af->frame->data), af->frame->nb_samples);

	if (is->muted && data_size > 0)
		memset(buffer_audio, 0, data_size); //mute
	
	play_buf(buffer_audio, data_size);

	audio_clock0 = is->audio_clock;
	/* update the audio clock with the pts */
	if (!isnan(af->pts))
	{
		is->audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
		//qDebug("audio: clock=%0.3f pts=%0.3f, frame:%0.3f\n", is->audio_clock, af->pts, (double)af->frame->nb_samples / af->frame->sample_rate);
	}
	else {
		is->audio_clock = NAN;
	}
	is->audio_clock_serial = af->serial;

	emit update_play_time();

#if (!NDEBUG && PRINT_PACKETQUEUE_INFO)
	{
		static double last_clock;
		qDebug("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
			is->audio_clock - last_clock,
			is->audio_clock, audio_clock0);
		last_clock = is->audio_clock;
	}
#endif

	return data_size;
}

bool AudioPlayThread::init_resample_param(AVCodecContext* pAudio)
{
	if (pAudio) {
		struct SwrContext* swrCtx = swr_alloc_set_opts(NULL,
			pAudio->channel_layout, AV_SAMPLE_FMT_S16, pAudio->sample_rate,
			pAudio->channel_layout, pAudio->sample_fmt, pAudio->sample_rate,
			0, nullptr);
		swr_init(swrCtx);

		m_audioResample.swrCtx = swrCtx;
		return true;
	}
	return false;
}

void AudioPlayThread::final_resample_param()
{
	swr_free(&m_audioResample.swrCtx);
}

void AudioPlayThread::stop_thread()
{
	m_bExitThread = true;
	wait();
}

void AudioPlayThread::wait_stop_thread()
{
}

void AudioPlayThread::pause_thread()
{
}