/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-trigger
 * Created on: 31 июл. 2021 г.
 *
 * lsp-plugins-trigger is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-trigger is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-trigger. If not, see <https://www.gnu.org/licenses/>.
 */

#include <private/plugins/trigger_kernel.h>
#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/atomic.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/dsp-units/misc/fade.h>
#include <lsp-plug.in/dsp/dsp.h>

#include <lsp-plug.in/shared/debug.h>

namespace lsp
{
    namespace plugins
    {
        //-------------------------------------------------------------------------
        trigger_kernel::AFLoader::AFLoader(trigger_kernel *base, afile_t *descr)
        {
            pCore       = base;
            pFile       = descr;
        }

        trigger_kernel::AFLoader::~AFLoader()
        {
            pCore       = NULL;
            pFile       = NULL;
        }

        status_t trigger_kernel::AFLoader::run()
        {
            dsp::context_t ctx;
            dsp::start(&ctx);
            lsp_finally { dsp::finish(&ctx); };

            return pCore->load_file(pFile);
        };

        void trigger_kernel::AFLoader::dump(dspu::IStateDumper *v) const
        {
            v->write("pCore", pCore);
            v->write("pFile", pFile);
        }

        //-------------------------------------------------------------------------
        trigger_kernel::AFRenderer::AFRenderer(trigger_kernel *base, afile_t *descr)
        {
            pCore       = base;
            pFile       = descr;
        }

        trigger_kernel::AFRenderer::~AFRenderer()
        {
            pCore       = NULL;
            pFile       = NULL;
        }

        status_t trigger_kernel::AFRenderer::run()
        {
            dsp::context_t ctx;
            dsp::start(&ctx);
            lsp_finally { dsp::finish(&ctx); };

            return pCore->render_sample(pFile);
        };

        void trigger_kernel::AFRenderer::dump(dspu::IStateDumper *v) const
        {
            v->write("pCore", pCore);
            v->write("pFile", pFile);
        }

        //-------------------------------------------------------------------------
        trigger_kernel::GCTask::GCTask(trigger_kernel *base)
        {
            pCore       = base;
        }

        trigger_kernel::GCTask::~GCTask()
        {
            pCore       = NULL;
        }

        status_t trigger_kernel::GCTask::run()
        {
            pCore->perform_gc();
            return STATUS_OK;
        }

        void trigger_kernel::GCTask::dump(dspu::IStateDumper *v) const
        {
            v->write("pCore", pCore);
        }

        //-------------------------------------------------------------------------
        trigger_kernel::trigger_kernel():
            sGCTask(this)
        {
            pExecutor       = NULL;
            vFiles          = NULL;
            vActive         = NULL;
            pGCList         = NULL;
            nFiles          = 0;
            nActive         = 0;
            nChannels       = 0;
            vBuffer         = NULL;
            bBypass         = false;
            bReorder        = false;
            fFadeout        = 10.0f;
            fDynamics       = meta::trigger_metadata::DYNA_DFL;
            fDrift          = meta::trigger_metadata::DRIFT_DFL;
            nSampleRate     = 0;

            pDynamics       = NULL;
            pDrift          = NULL;
            pActivity       = NULL;
            pData           = NULL;
        }

        trigger_kernel::~trigger_kernel()
        {
            lsp_trace("this = %p", this);
            destroy_state();
        }

        void trigger_kernel::set_fadeout(float length)
        {
            fFadeout        = length;
        }

        bool trigger_kernel::init(ipc::IExecutor *executor, size_t files, size_t channels)
        {
            // Validate parameters
            channels        = lsp_min(channels, meta::trigger_metadata::TRACKS_MAX);

            // Now we may store values
            nFiles          = files;
            nChannels       = channels;
            bReorder        = true;
            nActive         = 0;
            pExecutor       = executor;

            // Now determine object sizes
            size_t afile_szof           = align_size(sizeof(afile_t) * files, DEFAULT_ALIGN);
            size_t vactive_szof         = align_size(sizeof(afile_t *) * files, DEFAULT_ALIGN);
            size_t vbuffer_szof         = align_size(sizeof(float) * meta::trigger_metadata::BUFFER_SIZE, DEFAULT_ALIGN);

            // Allocate raw chunk and link data
            size_t allocate             = afile_szof + vactive_szof + vbuffer_szof;
            uint8_t *ptr                = alloc_aligned<uint8_t>(pData, allocate);
            if (ptr == NULL)
                return false;
            lsp_guard_assert(
                uint8_t *tail               = &ptr[allocate];
            );

            // Allocate files
            vFiles                      = advance_ptr_bytes<afile_t>(ptr, afile_szof);
            vActive                     = advance_ptr_bytes<afile_t *>(ptr, vactive_szof);
            vBuffer                     = advance_ptr_bytes<float>(ptr, vbuffer_szof);

            for (size_t i=0; i<files; ++i)
            {
                afile_t *af                 = &vFiles[i];

                af->nID                     = i;
                af->pLoader                 = NULL;
                af->pRenderer               = NULL;

                af->sListen.construct();
                af->sStop.construct();
                af->sNoteOn.construct();
                af->pOriginal               = NULL;
                af->pProcessed              = NULL;
                for (size_t j=0; j<meta::trigger_metadata::TRACKS_MAX; ++j)
                    af->vThumbs[j]              = NULL;

                for (size_t i=0; i<4; ++i)
                    af->vPlaybacks[i].construct();

                af->nUpdateReq              = 0;
                af->nUpdateResp             = 0;
                af->bSync                   = false;
                af->fVelocity               = 1.0f;
                af->fPitch                  = 0.0f;
                af->fHeadCut                = 0.0f;
                af->fTailCut                = 0.0f;
                af->fFadeIn                 = 0.0f;
                af->fFadeOut                = 0.0f;
                af->bReverse                = false;
                af->fPreDelay               = meta::trigger_metadata::PREDELAY_DFL;
                af->sListen.init();
                af->sStop.init();
                af->bOn                     = true;
                af->fMakeup                 = 1.0f;
                af->fLength                 = 0.0f;
                af->nStatus                 = STATUS_UNSPECIFIED;

                af->pFile                   = NULL;
                af->pPitch                  = NULL;
                af->pHeadCut                = NULL;
                af->pTailCut                = NULL;
                af->pFadeIn                 = NULL;
                af->pFadeOut                = NULL;
                af->pVelocity               = NULL;
                af->pMakeup                 = NULL;
                af->pPreDelay               = NULL;
                af->pOn                     = NULL;
                af->pListen                 = NULL;
                af->pStop                   = NULL;
                af->pReverse                = NULL;
                af->pLength                 = NULL;
                af->pStatus                 = NULL;
                af->pMesh                   = NULL;
                af->pActive                 = NULL;
                af->pNoteOn                 = NULL;

                for (size_t j=0; j < meta::trigger_metadata::TRACKS_MAX; ++j)
                {
                    af->fGains[j]               = 1.0f;
                    af->pGains[j]               = NULL;
                }

                vActive[i]                  = NULL;
            }

            // Create additional objects: tasks for file loading
            lsp_trace("Create loaders");
            for (size_t i=0; i<files; ++i)
            {
                afile_t  *af        = &vFiles[i];

                // Create loader
                af->pLoader         = new AFLoader(this, af);
                if (af->pLoader == NULL)
                {
                    destroy_state();
                    return false;
                }

                // Create renderer
                af->pRenderer       = new AFRenderer(this, af);
                if (af->pRenderer == NULL)
                {
                    destroy_state();
                    return false;
                }
            }

            // Assert
            lsp_assert(ptr <= tail);

            // Initialize channels
            lsp_trace("Initialize channels");
            for (size_t i=0; i<nChannels; ++i)
            {
                if (!vChannels[i].init(nFiles, meta::trigger_metadata::PLAYBACKS_MAX))
                {
                    destroy_state();
                    return false;
                }
            }

            return true;
        }

        size_t trigger_kernel::bind(plug::IPort **ports, size_t port_id, bool dynamics)
        {
            if (dynamics)
            {
                lsp_trace("Binding dynamics and drifting...");
                BIND_PORT(pDynamics);
                BIND_PORT(pDrift);
            }

            SKIP_PORT("Sample selector");

            // Iterate each file
            for (size_t i=0; i<nFiles; ++i)
            {
                lsp_trace("Binding sample %d", int(i));

                afile_t *af             = &vFiles[i];
                // Allocate files
                BIND_PORT(af->pFile);
                BIND_PORT(af->pPitch);
                BIND_PORT(af->pHeadCut);
                BIND_PORT(af->pTailCut);
                BIND_PORT(af->pFadeIn);
                BIND_PORT(af->pFadeOut);
                BIND_PORT(af->pMakeup);
                BIND_PORT(af->pVelocity);
                BIND_PORT(af->pPreDelay);
                BIND_PORT(af->pOn);
                BIND_PORT(af->pListen);
                BIND_PORT(af->pStop);
                BIND_PORT(af->pReverse);

                for (size_t j=0; j<nChannels; ++j)
                    BIND_PORT(af->pGains[j]);

                BIND_PORT(af->pActive);
                BIND_PORT(af->pNoteOn);
                BIND_PORT(af->pLength);
                BIND_PORT(af->pStatus);
                BIND_PORT(af->pMesh);
            }

            // Initialize randomizer
            sRandom.init();

            lsp_trace("Init OK");

            return port_id;
        }

        void trigger_kernel::bind_activity(plug::IPort *activity)
        {
            lsp_trace("Binding activity...");
            pActivity       = activity;
        }

        void trigger_kernel::destroy_sample(dspu::Sample * &sample)
        {
            if (sample == NULL)
                return;

            sample->destroy();
            delete sample;
            lsp_trace("Destroyed sample %p", sample);
            sample  = NULL;
        }

        void trigger_kernel::unload_afile(afile_t *af)
        {
            // Destroy original sample if present
            destroy_sample(af->pOriginal);
            destroy_sample(af->pProcessed);

            // Destroy pointer to thumbnails
            if (af->vThumbs[0])
            {
                free(af->vThumbs[0]);
                for (size_t i=0; i<meta::trigger_metadata::TRACKS_MAX; ++i)
                    af->vThumbs[i]              = NULL;
            }
        }

        void trigger_kernel::destroy_afile(afile_t *af)
        {
            af->sListen.destroy();
            af->sStop.destroy();
            af->sNoteOn.destroy();

            for (size_t i=0; i<4; ++i)
                af->vPlaybacks[i].destroy();

            // Delete audio file loader
            if (af->pLoader != NULL)
            {
                delete af->pLoader;
                af->pLoader = NULL;
            }

            // Delete audio file renderer
            if (af->pRenderer != NULL)
            {
                delete af->pRenderer;
                af->pRenderer = NULL;
            }

            // Destroy all sample-related data
            unload_afile(af);

            // Active sample is bound to the sampler, controlled by GC
            af->pActive     = NULL;
        }

        void trigger_kernel::destroy_samples(dspu::Sample *gc_list)
        {
            // Iterate over the list and destroy each sample in the list
            while (gc_list != NULL)
            {
                dspu::Sample *next = gc_list->gc_next();
                destroy_sample(gc_list);
                gc_list = next;
            }
        }

        void trigger_kernel::perform_gc()
        {
            dspu::Sample *gc_list = lsp::atomic_swap(&pGCList, NULL);
            lsp_trace("gc_list = %p", gc_list);
            destroy_samples(gc_list);
        }

        void trigger_kernel::destroy_state()
        {
            // Perform garbage collection for each channel
            for (size_t i=0; i<nChannels; ++i)
            {
                dspu::SamplePlayer *sp = &vChannels[i];
                dspu::Sample *gc_list = sp->destroy(false);
                destroy_samples(gc_list);
            }

            // Destroy audio files
            if (vFiles != NULL)
            {
                for (size_t i=0; i<nFiles;++i)
                    destroy_afile(&vFiles[i]);
            }

            // Perform pending gabrage collection
            perform_gc();

            // Drop all preallocated data
            free_aligned(pData);

            // Foget variables
            vFiles          = NULL;
            vActive         = NULL;
            vBuffer         = NULL;
            pExecutor       = NULL;
            nFiles          = 0;
            nChannels       = 0;
            bReorder        = false;
            bBypass         = false;

            pDynamics       = NULL;
            pDrift          = NULL;
        }

        void trigger_kernel::destroy()
        {
            destroy_state();
        }

        template <class T>
        void trigger_kernel::commit_afile_value(afile_t *af, T & field, plug::IPort *port)
        {
            const T temp = port->value();
            if (temp != field)
            {
                field       = temp;
                ++af->nUpdateReq;
            }
        }

        void trigger_kernel::commit_afile_value(afile_t *af, bool & field, plug::IPort *port)
        {
            const bool temp = port->value() >= 0.5f;
            if (temp != field)
            {
                field       = temp;
                ++af->nUpdateReq;
            }
        }

        void trigger_kernel::update_settings()
        {
            // Process file load requests
            for (size_t i=0; i<nFiles; ++i)
            {
                // Get descriptor
                afile_t *af             = &vFiles[i];
                if (af->pFile == NULL)
                    continue;

                // Get path
                plug::path_t *path = af->pFile->buffer<plug::path_t>();
                if ((path == NULL) || (!path->pending()))
                    continue;

                // Check task state
                if (af->pLoader->idle())
                {
                    // Try to submit task
                    if (pExecutor->submit(af->pLoader))
                    {
                        af->nStatus     = STATUS_LOADING;
                        lsp_trace("successfully submitted task");
                        path->accept();
                    }
                }
            }

            // Update note and octave
            lsp_trace("Initializing samples...");

            // Iterate all samples
            for (size_t i=0; i<nFiles; ++i)
            {
                afile_t *af         = &vFiles[i];

                // On/off switch
                bool on             = (af->pOn->value() >= 0.5f);
                if (af->bOn != on)
                {
                    af->bOn             = on;
                    bReorder            = true;
                }

                // Pre-delay gain
                af->fPreDelay       = af->pPreDelay->value();

                // Listen trigger
    //            lsp_trace("submit listen%d = %f", int(i), af->pListen->getValue());
                af->sListen.submit(af->pListen->value());
                af->sStop.submit(af->pStop->value());
    //            lsp_trace("listen[%d].pending = %s", int(i), (af->sListen.pending()) ? "true" : "false");

                // Makeup gain + mix gain
                af->fMakeup         = (af->pMakeup != NULL) ? af->pMakeup->value() : 1.0f;
                if (nChannels == 1)
                    af->fGains[0]       = af->pGains[0]->value();
                else if (nChannels == 2)
                {
                    af->fGains[0]       = (100.0f - af->pGains[0]->value()) * 0.005f;
                    af->fGains[1]       = (af->pGains[1]->value() + 100.0f) * 0.005f;
                }
                else
                {
                    for (size_t j=0; j<nChannels; ++j)
                        af->fGains[j]       = af->pGains[j]->value();
                }
    //            #ifdef LSP_TRACE
    //                for (size_t j=0; j<nChannels; ++j)
    //                    lsp_trace("gains[%d,%d] = %f", int(i), int(j), af->fGains[j]);
    //            #endif

                // Update velocity
                float value     = af->pVelocity->value();
                if (value != af->fVelocity)
                {
                    af->fVelocity   = value;
                    bReorder        = true;
                }

                // Update sample parameters
                commit_afile_value(af, af->fVelocity, af->pVelocity);
                commit_afile_value(af, af->fPitch, af->pPitch);
                commit_afile_value(af, af->fHeadCut, af->pHeadCut);
                commit_afile_value(af, af->fTailCut, af->pTailCut);
                commit_afile_value(af, af->fFadeIn, af->pFadeIn);
                commit_afile_value(af, af->fFadeOut, af->pFadeOut);
                commit_afile_value(af, af->bReverse, af->pReverse);
            }

            // Get humanisation parameters
            fDynamics       = (pDynamics != NULL) ? pDynamics->value() * 0.01f : 0.0f; // fDynamics = 0..1.0
            fDrift          = (pDrift != NULL)    ? pDrift->value() : 0.0f;
        }

        void trigger_kernel::sync_samples_with_ui()
        {
            // Iterate all samples
            for (size_t i=0; i<nFiles; ++i)
            {
                afile_t *af         = &vFiles[i];
                af->bSync           = true;
            }
        }

        void trigger_kernel::update_sample_rate(long sr)
        {
            // Store new sample rate
            nSampleRate     = sr;

            // Update activity counter
            sActivity.init(sr);

            for (size_t i=0; i<nFiles; ++i)
                vFiles[i].sNoteOn.init(sr);
        }

        status_t trigger_kernel::load_file(afile_t *file)
        {
            // Validate arguments
            if ((file == NULL) || (file->pFile == NULL))
                return STATUS_UNKNOWN_ERR;

            unload_afile(file);

            // Get path
            plug::path_t *path      = file->pFile->buffer<plug::path_t>();
            if (path == NULL)
                return STATUS_UNKNOWN_ERR;

            // Get file name
            const char *fname   = path->path();
            if (strlen(fname) <= 0)
                return STATUS_UNSPECIFIED;

            // Load audio file
            dspu::Sample *source    = new dspu::Sample();
            if (source == NULL)
                return STATUS_NO_MEM;
            lsp_finally { destroy_sample(source); };

            status_t status = source->load(fname, meta::trigger_metadata::SAMPLE_LENGTH_MAX * 0.001f);
            if (status != STATUS_OK)
            {
                lsp_trace("load failed: status=%d (%s)", status, get_status(status));
                return status;
            }
            const size_t channels   = lsp_min(nChannels, source->channels());
            if (!source->set_channels(channels))
            {
                lsp_trace("failed to resize source sample to %d channels", int(channels));
                return status;
            }

            // Initialize thumbnails
            float *thumbs           = static_cast<float *>(malloc(sizeof(float) * channels * meta::trigger_metadata::MESH_SIZE));
            if (thumbs == NULL)
                return STATUS_NO_MEM;

            for (size_t i=0; i<channels; ++i)
            {
                file->vThumbs[i]        = thumbs;
                thumbs                 += meta::trigger_metadata::MESH_SIZE;
            }

            // Commit result
            lsp_trace("file successful loaded: %s", fname);
            lsp::swap(file->pOriginal, source);

            return STATUS_OK;
        }

        status_t trigger_kernel::render_sample(afile_t *af)
        {
            // Validate arguments
            if (af == NULL)
                return STATUS_UNKNOWN_ERR;

            // Get maximum sample count
            dspu::Sample *src       = af->pOriginal;
            if (src == NULL)
                return STATUS_UNSPECIFIED;

            // Copy data of original sample to temporary sample and perform resampling
            dspu::Sample temp;
            size_t channels         = lsp_min(nChannels, src->channels());
            size_t sample_rate_dst  = nSampleRate * dspu::semitones_to_frequency_shift(-af->fPitch);
            if (temp.copy(src) != STATUS_OK)
            {
                lsp_warn("Error copying source sample");
                return STATUS_NO_MEM;
            }
            if (temp.resample(sample_rate_dst) != STATUS_OK)
            {
                lsp_warn("Error resampling source sample");
                return STATUS_NO_MEM;
            }

            // Determine the normalizing factor
            float abs_max       = 0.0f;
            for (size_t i=0; i<channels; ++i)
            {
                // Determine the maximum amplitude
                float a_max             = dsp::abs_max(temp.channel(i), temp.length());
                abs_max                 = lsp_max(abs_max, a_max);
            }
            float norming       = (abs_max != 0.0f) ? 1.0f / abs_max : 1.0f;

            // Compute the overall sample length
            ssize_t head        = dspu::millis_to_samples(sample_rate_dst, af->fHeadCut);
            ssize_t tail        = dspu::millis_to_samples(sample_rate_dst, af->fTailCut);
            ssize_t max_samples = lsp_max(0, ssize_t(temp.length() - head - tail));
            ssize_t fade_in     = dspu::millis_to_samples(nSampleRate, af->fFadeIn);
            ssize_t fade_out    = dspu::millis_to_samples(nSampleRate, af->fFadeOut);

            // Initialize target sample
            dspu::Sample *out   = new dspu::Sample();
            if (out == NULL)
                return STATUS_NO_MEM;
            lsp_finally { destroy_sample(out); };
            if (!out->init(channels, max_samples, max_samples))
            {
                lsp_warn("Error initializing playback sample");
                return STATUS_NO_MEM;
            }

            // Re-render playback sample from temporary sample
            for (size_t j=0; j<channels; ++j)
            {
                float *dst          = out->channel(j);
                const float *src    = temp.channel(j);

                if (af->bReverse)
                {
                    dsp::reverse2(dst, &src[tail], max_samples);
                    dspu::fade_in(dst, dst, fade_in, max_samples);
                }
                else
                    dspu::fade_in(dst, &src[head], fade_in, max_samples);

                dspu::fade_out(dst, dst, fade_out, max_samples);


                // Now render thumbnail
                src                 = dst;
                dst                 = af->vThumbs[j];
                for (size_t k=0; k<meta::trigger_metadata::MESH_SIZE; ++k)
                {
                    size_t first    = (k * max_samples) / meta::trigger_metadata::MESH_SIZE;
                    size_t last     = ((k + 1) * max_samples) / meta::trigger_metadata::MESH_SIZE;
                    if (first < last)
                        dst[k]          = dsp::abs_max(&src[first], last - first);
                    else
                        dst[k]          = fabs(src[first]);
                }

                // Normalize graph if possible
                if (norming != 1.0f)
                    dsp::mul_k2(dst, norming, meta::trigger_metadata::MESH_SIZE);
            }

            // Commit the new sample to the processed
            lsp::swap(out, af->pProcessed);

            return STATUS_OK;
        }

        void trigger_kernel::play_sample(const afile_t *af, float gain, size_t delay)
        {
            lsp_trace("id=%d, gain=%f, delay=%d", int(af->nID), gain, int(delay));

            // Obtain the sample that will be used for playback
            dspu::Sample *s = vChannels[0].get(af->nID);
            if (s == NULL)
                return;

            // Scale the final output gain
            gain    *= af->fMakeup;

            if (nChannels == 1)
            {
                lsp_trace("channels[%d].play(%d, %d, %f, %d)", int(0), int(af->nID), int(0), gain * af->fGains[0], int(delay));
                vChannels[0].play(af->nID, 0, gain * af->fGains[0], delay);
            }
            else if (nChannels == 2)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    size_t j=i^1; // j = (i + 1) % 2
                    const size_t channel = i % s->channels();

                    lsp_trace("channels[%d].play(%d, %d, %f, %d)", int(i), int(af->nID), int(i), gain * af->fGains[i], int(delay));
                    vChannels[i].play(af->nID, channel, gain * af->fGains[i], delay);
                    lsp_trace("channels[%d].play(%d, %d, %f, %d)", int(j), int(af->nID), int(i), gain * (1.0f - af->fGains[i]), int(delay));
                    vChannels[j].play(af->nID, channel, gain * (1.0f - af->fGains[i]), delay);
                }
            }
            else
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    const size_t channel = i % s->channels();

                    lsp_trace("channels[%d].play(%d, %d, %f, %d)", int(i), int(af->nID), int(i), gain * af->fGains[i], int(delay));
                    vChannels[i].play(af->nID, channel, gain * af->fGains[i], delay);
                }
            }
        }

        void trigger_kernel::cancel_sample(const afile_t *af, size_t fadeout, size_t delay)
        {
            lsp_trace("id=%d, delay=%d", int(af->nID), int(delay));

            // Cancel all playbacks
            for (size_t i=0; i<nChannels; ++i)
            {
                lsp_trace("channels[%d].cancel(%d, %d, %d)", int(af->nID), int(i), int(fadeout), int(delay));
                vChannels[i].cancel_all(af->nID, i, fadeout, delay);
            }
        }

        void trigger_kernel::trigger_on(size_t timestamp, float level)
        {
            if (nActive <= 0)
                return;

            // Binary search of sample
            lsp_trace("normalized velocity = %f", level);
            level      *=   100.0f; // Make velocity in percentage
            ssize_t f_first = 0, f_last = nActive-1;
            while (f_last > f_first)
            {
                ssize_t f_mid = (f_last + f_first) >> 1;
                if (level <= vActive[f_mid]->fVelocity)
                    f_last  = f_mid;
                else
                    f_first = f_mid + 1;
            }
            if (f_last < 0)
                f_last      = 0;
            else if (f_last >= ssize_t(nActive))
                f_last      = nActive - 1;

            // Get the file and ajdust gain
            afile_t *af     = vActive[f_last];
            size_t delay    = dspu::millis_to_samples(nSampleRate, af->fPreDelay) + timestamp;

            lsp_trace("f_last=%d, af->id=%d, af->velocity=%.3f", int(f_last), int(af->nID), af->fVelocity);

            // Apply changes to all ports
            if (af->fVelocity > 0.0f)
            {
                // Apply 'Humanisation' parameters
                level       = level * ((1.0f - fDynamics*0.5) + fDynamics * sRandom.random(dspu::RND_EXP)) / af->fVelocity;
                delay      += dspu::millis_to_samples(nSampleRate, fDrift) * sRandom.random(dspu::RND_EXP);

                // Play sample
                play_sample(af, level, delay);

                // Trigger the note On indicator
                af->sNoteOn.blink();
                sActivity.blink();
            }
        }

        void trigger_kernel::trigger_off(size_t timestamp, float level)
        {
            if (nActive <= 0)
                return;

            size_t delay    = timestamp;
            size_t fadeout  = dspu::millis_to_samples(nSampleRate, fFadeout);

            for (size_t i=0; i<nActive; ++i)
                cancel_sample(vActive[i], fadeout, delay);
        }

        void trigger_kernel::trigger_stop(size_t timestamp)
        {
            // Apply changes to all ports
            for (size_t j=0; j<nChannels; ++j)
                vChannels[j].stop();
        }

        void trigger_kernel::process_file_load_requests()
        {
            for (size_t i=0; i<nFiles; ++i)
            {
                // Get descriptor
                afile_t *af             = &vFiles[i];
                if (af->pFile == NULL)
                    continue;

                // Get path
                plug::path_t *path = af->pFile->buffer<plug::path_t>();
                if (path == NULL)
                    continue;

                // If there is new load request and loader is idle, then wake up the loader
                if ((path->pending()) && (af->pLoader->idle()) && (af->pRenderer->idle()))
                {
                    // Try to submit task
                    if (pExecutor->submit(af->pLoader))
                    {
                        ++af->nUpdateReq;
                        af->nStatus     = STATUS_LOADING;
                        lsp_trace("successfully submitted loader task");
                        path->accept();
                    }
                }
                else if ((path->accepted()) && (af->pLoader->completed()))
                {
                    // Commit the result
                    af->nStatus     = af->pLoader->code();
                    af->fLength     = (af->nStatus == STATUS_OK) ? af->pOriginal->duration() * 1000.0f : 0.0f;

                    // Trigger the sample for update and the state for reorder
                    ++af->nUpdateReq;
                    bReorder        = true;

                    // Now we can surely commit changes and reset task state
                    path->commit();
                    af->pLoader->reset();
                }
            }
        }

        void trigger_kernel::process_file_render_requests()
        {
            for (size_t i=0; i<nFiles; ++i)
            {
                // Get descriptor
                afile_t *af         = &vFiles[i];
                if (af->pFile == NULL)
                    continue;

                // Get path and check task state
                if ((af->nUpdateReq != af->nUpdateResp) && (af->pRenderer->idle()) && (af->pLoader->idle()))
                {
                    if (af->pOriginal == NULL)
                    {
                        af->nUpdateResp     = af->nUpdateReq;
                        af->pProcessed      = NULL;

                        // Unbind sample for all channels
                        for (size_t j=0; j<nChannels; ++j)
                            vChannels[j].unbind(af->nID);

                        af->bSync           = true;
                    }
                    else if (pExecutor->submit(af->pRenderer))
                    {
                        // Try to submit task
                        af->nUpdateResp     = af->nUpdateReq;
                        lsp_trace("successfully submitted renderer task");
                    }
                }
                else if (af->pRenderer->completed())
                {
                    // Commit changes if there is no more pending tasks
                    if (af->nUpdateReq == af->nUpdateResp)
                    {
                        // Bind sample for all channels
                        for (size_t j=0; j<nChannels; ++j)
                            vChannels[j].bind(af->nID, af->pProcessed);

                        // The sample is now under the garbage control inside of the sample player
                        af->pProcessed      = NULL;
                    }

                    af->pRenderer->reset();
                    af->bSync           = true;
                }
            }
        }

        void trigger_kernel::process_gc_tasks()
        {
            if (sGCTask.completed())
                sGCTask.reset();

            if (sGCTask.idle())
            {
                // Obtain the list of samples for destroy
                if (pGCList == NULL)
                {
                    for (size_t i=0; i<meta::trigger_metadata::TRACKS_MAX; ++i)
                        if ((pGCList = vChannels[i].gc()) != NULL)
                            break;
                }

                if (pGCList != NULL)
                    pExecutor->submit(&sGCTask);
            }
        }

        void trigger_kernel::reorder_samples()
        {
            if (!bReorder)
                return;
            bReorder = false;

            lsp_trace("Reordering active files");

            // Compute the list of active files
            nActive     = 0;
            for (size_t i=0; i<nFiles; ++i)
            {
                if (!vFiles[i].bOn)
                    continue;
                if (vFiles[i].pOriginal == NULL)
                    continue;

                lsp_trace("file %d is active", int(nActive));
                vActive[nActive++]  = &vFiles[i];
            }

            // Sort the list of active files
            if (nActive > 1)
            {
                for (size_t i=0; i<(nActive-1); ++i)
                    for (size_t j=i+1; j<nActive; ++j)
                        if (vActive[i]->fVelocity > vActive[j]->fVelocity)
                            lsp::swap(vActive[i], vActive[j]);
            }

            #ifdef LSP_TRACE
                for (size_t i=0; i<nActive; ++i)
                    lsp_trace("active file #%d: velocity=%.3f", int(vActive[i]->nID), vActive[i]->fVelocity);
            #endif /* LSP_TRACE */
        }

        void trigger_kernel::listen_sample(afile_t *af)
        {
            lsp_trace("id=%d", int(af->nID));

            // Obtain the sample that will be used for playback
            dspu::Sample *s = vChannels[0].get(af->nID);
            if (s == NULL)
                return;

            // Scale the final output gain
            const float gain    = af->fMakeup;
            size_t index        = 0;

            dspu::PlaySettings ps;

            if (nChannels == 1)
            {
                ps.set_channel(af->nID, 0);
                ps.set_playback(0, 0, gain * af->fGains[0]);
                af->vPlaybacks[index++]     = vChannels[0].play(&ps);
            }
            else
            {
                ps.set_channel(af->nID, 0);
                ps.set_playback(0, 0, gain * af->fGains[0]);
                af->vPlaybacks[index++]     = vChannels[0].play(&ps);
                ps.set_playback(0, 0, gain * (1.0f - af->fGains[0]));
                af->vPlaybacks[index++]     = vChannels[1].play(&ps);

                ps.set_channel(af->nID, 1);
                ps.set_playback(0, 0, gain * (1.0f - af->fGains[1]));
                af->vPlaybacks[index++]     = vChannels[0].play(&ps);
                ps.set_playback(0, 0, gain * af->fGains[1]);
                af->vPlaybacks[index++]     = vChannels[1].play(&ps);
            }
        }

        void trigger_kernel::cancel_listen(afile_t *af)
        {
            const size_t fadeout = dspu::millis_to_samples(nSampleRate, 5);
            for (size_t i=0; i<4; ++i)
                af->vPlaybacks[i].cancel(fadeout, 0);
        }

        void trigger_kernel::process_listen_events()
        {
            for (size_t i=0; i<nFiles; ++i)
            {
                // Get descriptor
                afile_t *af         = &vFiles[i];
                if (af->pFile == NULL)
                    continue;

                // Trigger the listen event
                if (af->sListen.pending())
                {
                    // Play sample
                    cancel_listen(af);
                    listen_sample(af);

                    // Update states
                    af->sListen.commit();
                    af->sNoteOn.blink();
                }

                if (af->sStop.pending())
                {
                    cancel_listen(af);
                    af->sStop.commit();
                }
            }
        }

        void trigger_kernel::play_samples(float **outs, const float **ins, size_t samples)
        {
            if (ins != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                    vChannels[i].process(outs[i], ins[i], samples, dspu::SAMPLER_ALL);
            }
            else
            {
                for (size_t i=0; i<nChannels; ++i)
                    vChannels[i].process(outs[i], samples, dspu::SAMPLER_ALL);
            }
        }

        void trigger_kernel::process(float **outs, const float **ins, size_t samples)
        {
            process_file_load_requests();
            process_file_render_requests();
            process_gc_tasks();
            reorder_samples();
            process_listen_events();
            play_samples(outs, ins, samples);
            output_parameters(samples);
        }

        void trigger_kernel::output_parameters(size_t samples)
        {
            // Update activity led output
            if (pActivity != NULL)
                pActivity->set_value(sActivity.process(samples));

            for (size_t i=0; i<nFiles; ++i)
            {
                afile_t *af         = &vFiles[i];

                // Output information about the file
                af->pLength->set_value(af->fLength);
                af->pStatus->set_value(af->nStatus);

                // Output information about the activity
                af->pNoteOn->set_value(af->sNoteOn.process(samples));

                // Get file sample
                dspu::Sample *active    = vChannels[0].get(af->nID);
                size_t channels         = (active != NULL) ? active->channels() : 0;
                channels                = lsp_min(channels, nChannels);

                // Output activity flag
                af->pActive->set_value(((af->bOn) && (channels > 0)) ? 1.0f : 0.0f);

                // Store file thumbnails to mesh
                plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(af->pMesh->buffer());
                if ((mesh == NULL) || (!mesh->isEmpty()) || (!af->bSync) || (!af->pLoader->idle()) || (!af->pRenderer->idle()))
                    continue;

                if ((channels > 0) && (af->vThumbs[0] != NULL))
                {
                    // Copy thumbnails
                    for (size_t j=0; j<channels; ++j)
                        dsp::copy(mesh->pvData[j], af->vThumbs[j], meta::trigger_metadata::MESH_SIZE);

                    mesh->data(channels, meta::trigger_metadata::MESH_SIZE);
                }
                else
                    mesh->data(0, 0);

                af->bSync           = false;
            }
        }

        void trigger_kernel::dump_afile(dspu::IStateDumper *v, const afile_t *f) const
        {
            v->write("nID", f->nID);
            v->write_object("pLoader", f->pLoader);
            v->write_object("pRenderer", f->pRenderer);
            v->write_object("sListen", &f->sListen);
            v->write_object("sStop", &f->sStop);
            v->write_object("sNoteOn", &f->sNoteOn);
            v->write_object("pOriginal", f->pOriginal);
            v->write_object("pProcessed", f->pProcessed);
            v->write("vThumbs", f->vThumbs);

            v->write_object_array("vPlaybacks", f->vPlaybacks, 4);

            v->write("nUpdateReq", f->nUpdateReq);
            v->write("nUpdateResp", f->nUpdateResp);
            v->write("bSync", f->bSync);
            v->write("fVelocity", f->fVelocity);
            v->write("fPitch", f->fPitch);
            v->write("fHeadCut", f->fHeadCut);
            v->write("fTailCut", f->fTailCut);
            v->write("fFadeIn", f->fFadeIn);
            v->write("fFadeOut", f->fFadeOut);
            v->write("bReverse", f->bReverse);
            v->write("fPreDelay", f->fPreDelay);
            v->write("fMakeup", f->fMakeup);
            v->writev("fGains", f->fGains, meta::trigger_metadata::TRACKS_MAX);
            v->write("fLength", f->fLength);
            v->write("nStatus", f->nStatus);
            v->write("bOn", f->bOn);

            v->write("pFile", f->pFile);
            v->write("pPitch", f->pPitch);
            v->write("pHeadCut", f->pHeadCut);
            v->write("pTailCut", f->pTailCut);
            v->write("pFadeIn", f->pFadeIn);
            v->write("pFadeOut", f->pFadeOut);
            v->write("pMakeup", f->pMakeup);
            v->write("pVelocity", f->pVelocity);
            v->write("pPreDelay", f->pPreDelay);
            v->write("pListen", f->pListen);
            v->write("pStop", f->pStop);
            v->write("pReverse", f->pReverse);
            v->writev("pGains", f->pGains, meta::trigger_metadata::TRACKS_MAX);
            v->write("pLength", f->pLength);
            v->write("pStatus", f->pStatus);
            v->write("pMesh", f->pMesh);
            v->write("pNoteOn", f->pNoteOn);
            v->write("pOn", f->pOn);
            v->write("pActive", f->pActive);
        }

        void trigger_kernel::dump(dspu::IStateDumper *v) const
        {
            v->write("pExecutor", pExecutor);
            v->write("pGCList", pExecutor);
            v->begin_array("vFiles", vFiles, nFiles);
            {
                for (size_t i=0; i<nFiles; ++i)
                {
                    v->begin_object(v, sizeof(afile_t));
                        dump_afile(v, &vFiles[i]);
                    v->end_object();
                }
            }
            v->end_array();

            v->writev("vActive", vActive, nActive);

            v->write_object_array("vChannels", vChannels, meta::trigger_metadata::TRACKS_MAX);
            v->write_object_array("vBypass", vBypass, meta::trigger_metadata::TRACKS_MAX);
            v->write_object("sActivity", &sActivity);
            v->write_object("sRandom", &sRandom);
            v->write_object("sGCTask", &sGCTask);

            v->write("nFiles", nFiles);
            v->write("nActive", nActive);
            v->write("nChannels", nChannels);
            v->write("vBuffer", vBuffer);
            v->write("bBypass", bBypass);
            v->write("bReorder", bReorder);
            v->write("fFadeout", fFadeout);
            v->write("fDynamics", fDynamics);
            v->write("fDrift", fDrift);
            v->write("nSampleRate", nSampleRate);

            v->write("pDynamics", pDynamics);
            v->write("pDrift", pDrift);
            v->write("pActivity", pActivity);
            v->write("pData", pData);
        }
    } /* namespace plugins */
} /* namespace lsp */

