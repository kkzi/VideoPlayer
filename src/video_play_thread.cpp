#include "video_play_thread.h"


int framedrop = 1;
static int64_t audio_callback_time;

VideoPlayThread::VideoPlayThread(QObject* parent, VideoState* pState)
	: QThread(parent)
	, m_pState(pState)
	, m_bExitThread(false)
{
	memset(&m_Resample, 0, sizeof(Video_Resample));
}

VideoPlayThread::~VideoPlayThread()
{
	stop_thread();
	final_resample_param();
}

void VideoPlayThread::run()
{
	assert(m_pState);
	VideoState* is = m_pState;
	double remaining_time = 0.0;

	for (;;)
	{
		if (m_bExitThread)
			break;

		if (is->abort_request)
			break;

		if (is->paused)	{
			msleep(10);
			continue;
		}

		if (remaining_time > 0.0)
			av_usleep((int64_t)(remaining_time * 1000000.0));

		remaining_time = REFRESH_RATE;
		if ((!is->paused || is->force_refresh))
			video_refresh(is, &remaining_time);
	}

	qDebug("--------video play thread exit.");
}

void VideoPlayThread::video_refresh(VideoState* is, double* remaining_time)
{
	double time;

	Frame* sp, * sp2;

	if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
		check_external_clock_speed(is);

	if (is->video_st)
	{
	retry:
		if (frame_queue_nb_remaining(&is->pictq) == 0) {
			// nothing to do, no picture to display in the queue
			*remaining_time = REFRESH_RATE;
		}
		else {
			double last_duration, duration, delay;
			Frame* vp, * lastvp;

			/* dequeue the picture */
			lastvp = frame_queue_peek_last(&is->pictq);
			vp = frame_queue_peek(&is->pictq);

			if (vp->serial != is->videoq.serial) {
				frame_queue_next(&is->pictq);
				goto retry;
			}

			if (lastvp->serial != vp->serial)
				is->frame_timer = av_gettime_relative() / 1000000.0;

			if (is->paused)
				goto display;

			/* compute nominal last_duration */
			last_duration = vp_duration(is, lastvp, vp);
			delay = compute_target_delay(last_duration, is);

			time = av_gettime_relative() / 1000000.0;
			if (time < is->frame_timer + delay) {
				*remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
				goto display;
			}

			is->frame_timer += delay;
			if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
				is->frame_timer = time;

			is->pictq.mutex->lock();
			if (!isnan(vp->pts))
				update_video_pts(is, vp->pts, vp->pos, vp->serial);
			is->pictq.mutex->unlock();

			if (frame_queue_nb_remaining(&is->pictq) > 1) {
				Frame* nextvp = frame_queue_peek_next(&is->pictq);
				duration = vp_duration(is, vp, nextvp);
				if (!is->step && (framedrop > 0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration) {
					is->frame_drops_late++;
					frame_queue_next(&is->pictq);
					goto retry;
				}
			}

			if (is->subtitle_st) {
				while (frame_queue_nb_remaining(&is->subpq) > 0) {
					sp = frame_queue_peek(&is->subpq);

					if (frame_queue_nb_remaining(&is->subpq) > 1)
						sp2 = frame_queue_peek_next(&is->subpq);
					else
						sp2 = NULL;

					if (sp->serial != is->subtitleq.serial
						|| (is->vidclk.pts > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
						|| (sp2 && is->vidclk.pts > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
					{
						if (sp->uploaded) {
							int i;
							for (i = 0; i < sp->sub.num_rects; i++) {
								/*AVSubtitleRect* sub_rect = sp->sub.rects[i];
								uint8_t* pixels;
								int pitch, j;

								if (!SDL_LockTexture(is->sub_texture, (SDL_Rect*)sub_rect, (void**)&pixels, &pitch)) {
									for (j = 0; j < sub_rect->h; j++, pixels += pitch)
										memset(pixels, 0, sub_rect->w << 2);
									SDL_UnlockTexture(is->sub_texture);
								}*/
							}
						}
						frame_queue_next(&is->subpq);
					}
					else {
						break;
					}
				}
			}

			frame_queue_next(&is->pictq);
			is->force_refresh = 1;

			if (is->step && !is->paused)
				stream_toggle_pause(is);
		}

	display:
		/* display picture */
		if (is->force_refresh && is->pictq.rindex_shown)
			video_display(is);
	}

	is->force_refresh = 0;
}

void VideoPlayThread::video_display(VideoState* is)
{
	if (is->audio_st && false)
		video_audio_display(is);
	else if (is->video_st)
		video_image_display(is);
}

void VideoPlayThread::video_audio_display(VideoState* s)
{
#if 0
	int i, i_start, x, y1, y, ys, delay, n, nb_display_channels;
	int ch, channels, h, h2;
	int64_t time_diff;
	int rdft_bits, nb_freq;

	for (rdft_bits = 1; (1 << rdft_bits) < 2 * s->height; rdft_bits++)
		;
	nb_freq = 1 << (rdft_bits - 1);

	/* compute display index : center on currently output samples */
	channels = s->audio_tgt.channels;
	nb_display_channels = channels;
	if (!s->paused) {
		int data_used = (2 * nb_freq);
		n = 2 * channels;
		delay = s->audio_write_buf_size;
		delay /= n;

		/* to be more precise, we take into account the time spent since
		   the last buffer computation */
		if (audio_callback_time) {
			time_diff = av_gettime_relative() - audio_callback_time;
			delay -= (time_diff * s->audio_tgt.freq) / 1000000;
		}

		delay += 2 * data_used;
		if (delay < data_used)
			delay = data_used;

		i_start = x = compute_mod(s->sample_array_index - delay * channels, SAMPLE_ARRAY_SIZE);
		/*if (s->show_mode == SHOW_MODE_WAVES) {
			h = INT_MIN;
			for (i = 0; i < 1000; i += channels) {
				int idx = (SAMPLE_ARRAY_SIZE + x - i) % SAMPLE_ARRAY_SIZE;
				int a = s->sample_array[idx];
				int b = s->sample_array[(idx + 4 * channels) % SAMPLE_ARRAY_SIZE];
				int c = s->sample_array[(idx + 5 * channels) % SAMPLE_ARRAY_SIZE];
				int d = s->sample_array[(idx + 9 * channels) % SAMPLE_ARRAY_SIZE];
				int score = a - d;
				if (h < score && (b ^ c) < 0) {
					h = score;
					i_start = idx;
				}
			}
		}*/

		s->last_i_start = i_start;
	}
	else {
		i_start = s->last_i_start;
	}

#if 0
	if (s->show_mode == SHOW_MODE_WAVES) {
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

		/* total height for one channel */
		h = s->height / nb_display_channels;
		/* graph height / 2 */
		h2 = (h * 9) / 20;
		for (ch = 0; ch < nb_display_channels; ch++) {
			i = i_start + ch;
			y1 = s->ytop + ch * h + (h / 2); /* position of center line */
			for (x = 0; x < s->width; x++) {
				y = (s->sample_array[i] * h2) >> 15;
				if (y < 0) {
					y = -y;
					ys = y1 - y;
				}
				else {
					ys = y1;
				}
				fill_rectangle(s->xleft + x, ys, 1, y);
				i += channels;
				if (i >= SAMPLE_ARRAY_SIZE)
					i -= SAMPLE_ARRAY_SIZE;
			}
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

		for (ch = 1; ch < nb_display_channels; ch++) {
			y = s->ytop + ch * h;
			fill_rectangle(s->xleft, y, s->width, 1);
		}
	}
	else {
		if (realloc_texture(&s->vis_texture, SDL_PIXELFORMAT_ARGB8888, s->width, s->height, SDL_BLENDMODE_NONE, 1) < 0)
			return;

		if (s->xpos >= s->width)
			s->xpos = 0;
		nb_display_channels = FFMIN(nb_display_channels, 2);
		if (rdft_bits != s->rdft_bits) {
			av_rdft_end(s->rdft);
			av_free(s->rdft_data);
			s->rdft = av_rdft_init(rdft_bits, DFT_R2C);
			s->rdft_bits = rdft_bits;
			s->rdft_data = av_malloc_array(nb_freq, 4 * sizeof(*s->rdft_data));
		}
		if (!s->rdft || !s->rdft_data) {
			av_log(NULL, AV_LOG_ERROR, "Failed to allocate buffers for RDFT, switching to waves display\n");
			s->show_mode = SHOW_MODE_WAVES;
		}
		else {
			FFTSample* data[2];
			SDL_Rect rect = { .x = s->xpos, .y = 0, .w = 1, .h = s->height };
			uint32_t* pixels;
			int pitch;
			for (ch = 0; ch < nb_display_channels; ch++) {
				data[ch] = s->rdft_data + 2 * nb_freq * ch;
				i = i_start + ch;
				for (x = 0; x < 2 * nb_freq; x++) {
					double w = (x - nb_freq) * (1.0 / nb_freq);
					data[ch][x] = s->sample_array[i] * (1.0 - w * w);
					i += channels;
					if (i >= SAMPLE_ARRAY_SIZE)
						i -= SAMPLE_ARRAY_SIZE;
				}
				av_rdft_calc(s->rdft, data[ch]);
			}
			/* Least efficient way to do this, we should of course
			 * directly access it but it is more than fast enough. */
			if (!SDL_LockTexture(s->vis_texture, &rect, (void**)&pixels, &pitch)) {
				pitch >>= 2;
				pixels += pitch * s->height;
				for (y = 0; y < s->height; y++) {
					double w = 1 / sqrt(nb_freq);
					int a = sqrt(w * sqrt(data[0][2 * y + 0] * data[0][2 * y + 0] + data[0][2 * y + 1] * data[0][2 * y + 1]));
					int b = (nb_display_channels == 2) ? sqrt(w * hypot(data[1][2 * y + 0], data[1][2 * y + 1]))
						: a;
					a = FFMIN(a, 255);
					b = FFMIN(b, 255);
					pixels -= pitch;
					*pixels = (a << 16) + (b << 8) + ((a + b) >> 1);
				}
				SDL_UnlockTexture(s->vis_texture);
			}
			SDL_RenderCopy(renderer, s->vis_texture, NULL, NULL);
		}
		if (!s->paused)
			s->xpos++;
	}
#endif
#endif
}


void VideoPlayThread::video_image_display(VideoState* is)
{
	Video_Resample* pResample = &m_Resample;

	Frame* vp = frame_queue_peek_last(&is->pictq);

	AVFrame* pFrameRGB = pResample->pFrameRGB; // dst
	AVCodecContext* pVideoCtx = is->viddec.avctx;
	AVFrame* pFrame = vp->frame;
	// AVPixelFormat fmt = (AVPixelFormat)pFrame->format; // 0
	// const char* fmt_name = av_get_sample_fmt_name(fmt);

	sws_scale(pResample->sws_ctx, (uint8_t const* const*)pFrame->data, pFrame->linesize, 0,
		pVideoCtx->height, pFrameRGB->data, pFrameRGB->linesize);

	QImage img(pVideoCtx->width, pVideoCtx->height, QImage::Format_RGB888);
	for (int y = 0; y < pVideoCtx->height; ++y) {
		memcpy(img.scanLine(y), pFrameRGB->data[0] + y * pFrameRGB->linesize[0], pVideoCtx->width * 3);
	}

	emit frame_ready(img);
}

bool VideoPlayThread::init_resample_param(AVCodecContext* pVideo)
{
	Video_Resample* pResample = &m_Resample;
	if (pVideo)
	{
		struct SwsContext* sws_ctx = sws_getContext(
			pVideo->width,
			pVideo->height,
			pVideo->pix_fmt,
			pVideo->width,
			pVideo->height,
			AV_PIX_FMT_RGB24,   // sws_scale destination color scheme
			SWS_BILINEAR,
			NULL,
			NULL,
			NULL
		);

		AVFrame* pFrameRGB = av_frame_alloc();
		if (pFrameRGB == NULL)
		{
			printf("Could not allocate rgb frame.\n");
			return false;
		}

		int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pVideo->width, pVideo->height, 32);
		uint8_t* buffer_RGB = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
		if (buffer_RGB == NULL)
		{
			printf("Could not allocate buffer.\n");
			return false;
		}

		av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer_RGB,
			AV_PIX_FMT_RGB24, pVideo->width, pVideo->height, 32);

		pResample->sws_ctx = sws_ctx;
		pResample->pFrameRGB = pFrameRGB;
		pResample->buffer_RGB = buffer_RGB;
		return true;
	}
	return false;
}

void VideoPlayThread::final_resample_param()
{
	Video_Resample* pResample = &m_Resample;
	//Free video resample context
	sws_freeContext(pResample->sws_ctx);

	// Free the RGB image
	av_free(pResample->buffer_RGB);
	av_frame_free(&pResample->pFrameRGB);
	av_free(pResample->pFrameRGB);
}

void VideoPlayThread::stop_thread()
{
	m_bExitThread = true;
	wait();
}

void VideoPlayThread::pause_thread()
{
}
