/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
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

#include <private/plugins/trigger.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/stdlib/math.h>

#include <lsp-plug.in/shared/id_colors.h>

#define TRACE_PORT(p) lsp_trace("  port id=%s", (p)->metadata()->id);

namespace lsp
{
    namespace plugins
    {
        //-------------------------------------------------------------------------
        // Plugin factory
        typedef struct plugin_settings_t
        {
            const meta::plugin_t   *metadata;
            uint8_t                 channels;
            bool                    midi;
        } plugin_settings_t;

        static const meta::plugin_t *plugins[] =
        {
            &meta::trigger_mono,
            &meta::trigger_stereo,
            &meta::trigger_midi_mono,
            &meta::trigger_midi_stereo
        };

        static const plugin_settings_t plugin_settings[] =
        {
            { &meta::trigger_mono,          1, false    },
            { &meta::trigger_stereo,        2, false    },
            { &meta::trigger_midi_mono,     1, true     },
            { &meta::trigger_midi_stereo,   2, true     },
            { NULL, 0, false }
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            for (const plugin_settings_t *s = plugin_settings; s->metadata != NULL; ++s)
                if (s->metadata == meta)
                    return new trigger(s->metadata, s->channels, s->midi);
            return NULL;
        }

        static plug::Factory factory(plugin_factory, plugins, 4);

        //-------------------------------------------------------------------------

        trigger::trigger(const meta::plugin_t *metadata, size_t channels, bool midi):
            plug::Module(metadata)
        {
            // Instantiation parameters
            vTmp                = NULL;
            nFiles              = meta::trigger_metadata::SAMPLE_FILES;
            nChannels           = channels;
            bMidiPorts          = midi;

            // Processors and buffers
            vTimePoints         = NULL;

            // Processing variables
            nCounter            = 0;
            nState              = T_OFF;
            fVelocity           = 0.0f;
            bFunctionActive     = true;
            bVelocityActive     = true;

            // Parameters
            nNote               = meta::trigger_metadata::MIDI_NOTE_DFL + meta::trigger_metadata::MIDI_OCTAVE_DFL * 12;
            nChannel            = meta::trigger_metadata::MIDI_CHANNEL_DFL;
            fDry                = 1.0f;
            fWet                = 1.0f;
            bPause              = false;
            bClear              = false;
            bUISync             = true;

            nDetectCounter      = 0;
            nReleaseCounter     = 0;
            fDetectLevel        = meta::trigger_metadata::DETECT_LEVEL_DFL;
            fDetectTime         = meta::trigger_metadata::DETECT_TIME_DFL;
            fReleaseLevel       = meta::trigger_metadata::RELEASE_LEVEL_DFL;
            fReleaseTime        = meta::trigger_metadata::RELEASE_TIME_DFL;
            fDynamics           = 0.0f;
            fDynaTop            = 1.0f;
            fDynaBottom         = 0.0f;
            pIDisplay           = NULL;

            // Control ports
            pFunction           = NULL;
            pFunctionLevel      = NULL;
            pFunctionActive     = NULL;
            pVelocity           = NULL;
            pVelocityLevel      = NULL;
            pVelocityActive     = NULL;
            pActive             = NULL;

            pMidiIn             = NULL;
            pMidiOut            = NULL;
            pChannel            = NULL;
            pNote               = NULL;
            pOctave             = NULL;
            pMidiNote           = NULL;

            pBypass             = NULL;
            pDry                = NULL;
            pWet                = NULL;
            pGain               = NULL;
            pPreamp             = NULL;
            pScHpfMode          = NULL;
            pScHpfFreq          = NULL;
            pScLpfMode          = NULL;
            pScLpfFreq          = NULL;

            pSource             = NULL;
            pMode               = NULL;
            pPause              = NULL;
            pClear              = NULL;
            pPreamp             = NULL;
            pDetectLevel        = NULL;
            pDetectTime         = NULL;
            pReleaseLevel       = NULL;
            pReleaseTime        = NULL;
            pDynamics           = NULL;
            pDynaRange1         = NULL;
            pDynaRange2         = NULL;
            pReactivity         = NULL;
            pReleaseValue       = NULL;
        }

        trigger::~trigger()
        {
            destroy();
        }

        void trigger::destroy()
        {
            // Destroy objects
            sSidechain.destroy();
            sScEq.destroy();
            sKernel.destroy();

            // Remove time points buffer
            if (vTimePoints != NULL)
            {
                delete [] vTimePoints;
                vTimePoints     = NULL;
            }

            for (size_t i=0; i<meta::trigger_metadata::TRACKS_MAX; ++i)
            {
                channel_t *tc   = &vChannels[i];
                tc->vCtl        = NULL;
                tc->pIn         = NULL;
                tc->pOut        = NULL;
            }

            vTmp        = NULL;

            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }
        }

        void trigger::init(plug::IWrapper *wrapper)
        {
            // Pass wrapper
            plug::Module::init(wrapper);

            if (!sSidechain.init(nChannels, meta::trigger_metadata::REACTIVITY_MAX))
                return;
            if (!sScEq.init(2, 12))
                return;
            sScEq.set_mode(dspu::EQM_IIR);
            sSidechain.set_pre_equalizer(&sScEq);

            // Get executor
            ipc::IExecutor *executor = wrapper->executor();

            // Initialize audio channels
            for (size_t i=0; i<meta::trigger_metadata::TRACKS_MAX; ++i)
            {
                channel_t *c        = &vChannels[i];
                c->vCtl             = NULL;
                c->pIn              = NULL;
                c->pOut             = NULL;
                c->pGraph           = NULL;
                c->pMeter           = NULL;
                c->pVisible         = NULL;
            }

            // Allocate buffer for time coordinates
            size_t allocate     = meta::trigger_metadata::HISTORY_MESH_SIZE + meta::trigger_metadata::BUFFER_SIZE*3;
            float *ctlbuf       = new float[allocate];
            if (ctlbuf == NULL)
                return;
            dsp::fill_zero(ctlbuf, allocate);

            vTimePoints         = ctlbuf;
            ctlbuf             += meta::trigger_metadata::HISTORY_MESH_SIZE;
            vTmp                = ctlbuf;
            ctlbuf             += meta::trigger_metadata::BUFFER_SIZE;

            // Fill time dots with values
            float step          = meta::trigger_metadata::HISTORY_TIME / meta::trigger_metadata::HISTORY_MESH_SIZE;
            for (size_t i=0; i < meta::trigger_metadata::HISTORY_MESH_SIZE; ++i)
                vTimePoints[i]      = (meta::trigger_metadata::HISTORY_MESH_SIZE - i - 1) * step;

            // Initialize trigger
            sKernel.init(executor, nFiles, nChannels);

            // Now we are ready to bind ports
            size_t port_id          = 0;

            // Bind audio inputs
            lsp_trace("Binding audio inputs...");
            for (size_t i=0; i<nChannels; ++i)
            {
                TRACE_PORT(vPorts[port_id]);
                vChannels[i].pIn        = vPorts[port_id++];
                vChannels[i].vCtl       = ctlbuf;
                ctlbuf                 += meta::trigger_metadata::BUFFER_SIZE;
            }

            // Bind audio outputs
            lsp_trace("Binding audio outputs...");
            for (size_t i=0; i<nChannels; ++i)
            {
                TRACE_PORT(vPorts[port_id]);
                vChannels[i].pOut       = vPorts[port_id++];
            }

            // Bind meters
            if (nChannels > 1)
            {
                lsp_trace("Binding source switch port...");
                TRACE_PORT(vPorts[port_id]);
                pSource             = vPorts[port_id++];
            }

            lsp_trace("Binding audio meters...");
            for (size_t i=0; i<nChannels; ++i)
            {
                TRACE_PORT(vPorts[port_id]);
                vChannels[i].pGraph     = vPorts[port_id++];
            }
            for (size_t i=0; i<nChannels; ++i)
            {
                TRACE_PORT(vPorts[port_id]);
                vChannels[i].pMeter     = vPorts[port_id++];
            }
            for (size_t i=0; i<nChannels; ++i)
            {
                TRACE_PORT(vPorts[port_id]);
                vChannels[i].pVisible   = vPorts[port_id++];
            }

            // Bind MIDI ports
            if (bMidiPorts)
            {
                lsp_trace("Binding MIDI ports...");
                TRACE_PORT(vPorts[port_id]);
                pMidiIn     = vPorts[port_id++];
                TRACE_PORT(vPorts[port_id]);
                pMidiOut    = vPorts[port_id++];
                TRACE_PORT(vPorts[port_id]);
                pChannel    = vPorts[port_id++];
                TRACE_PORT(vPorts[port_id]);
                pNote       = vPorts[port_id++];
                TRACE_PORT(vPorts[port_id]);
                pOctave     = vPorts[port_id++];
                TRACE_PORT(vPorts[port_id]);
                pMidiNote   = vPorts[port_id++];
            }

            // Skip area selector
            lsp_trace("Skipping Area selector...");
            TRACE_PORT(vPorts[port_id]);
            port_id     ++;

            // Bind ports
            lsp_trace("Binding Global ports...");
            TRACE_PORT(vPorts[port_id]);
            pBypass     = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pDry        = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pWet        = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pGain       = vPorts[port_id++];

            lsp_trace("Binding mode port...");
            TRACE_PORT(vPorts[port_id]);
            pMode               = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pPause              = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pClear              = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pPreamp             = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pScHpfMode          =   vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pScHpfFreq          =   vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pScLpfMode          =   vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pScLpfFreq          =   vPorts[port_id++];

            TRACE_PORT(vPorts[port_id]);
            pDetectLevel        = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pDetectTime         = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pReleaseLevel       = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pReleaseTime        = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pDynamics           = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pDynaRange1         = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pDynaRange2         = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pReactivity         = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pReleaseValue       = vPorts[port_id++];

            lsp_trace("Binding meters...");
            TRACE_PORT(vPorts[port_id]);
            pFunction           = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pFunctionLevel      = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pFunctionActive     = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pActive             = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pVelocity           = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pVelocityLevel      = vPorts[port_id++];
            TRACE_PORT(vPorts[port_id]);
            pVelocityActive     = vPorts[port_id++];

            // Bind kernel
            lsp_trace("Binding kernel ports...");
            port_id             = sKernel.bind(vPorts, port_id, false);
        }

        inline void trigger::update_counters()
        {
            if (fSampleRate <= 0)
                return;

            nDetectCounter      = dspu::millis_to_samples(fSampleRate, fDetectTime);
            nReleaseCounter     = dspu::millis_to_samples(fSampleRate, fReleaseTime);
        }

        void trigger::process_samples(const float *sc, size_t samples)
        {
            float max_level     = 0.0f, max_velocity  = 0.0f;

            // Process input data
            for (size_t i=0; i<samples; ++i)
            {
                // Get sample and log to function
                float level         = sc[i];
                if (level > max_level)
                    max_level           = level;
                sFunction.process(level);

                // Check trigger state
                switch (nState)
                {
                    case T_OFF: // Trigger is closed
                        if (level >= fDetectLevel) // Signal is growing, open trigger
                        {
                            // Mark trigger open
                            nCounter    = nDetectCounter;
                            nState      = T_DETECT;
                        }
                        break;
                    case T_DETECT:
                        if (level < fDetectLevel)
                            nState      = T_OFF;
                        else if ((nCounter--) <= 0)
                        {
                            // Calculate the velocity
                            fVelocity   = 0.5f * expf(fDynamics * logf(level/fDetectLevel));
                            float vel   = fVelocity;
                            if (vel >= fDynaTop) // Saturate to maximum
                                vel         = 1.0f;
                            else if (vel <= fDynaBottom) // Saturate to minimum
                                vel         = 0.0f;
                            else // Calculate the velocity based on logarithmic scale
                                vel         = logf(vel/fDynaBottom) / logf(fDynaTop/fDynaBottom);

                            // Trigger state ON
                            trigger_on(i, vel);
                            nState      = T_ON;

                            // Indicate that trigger is active
                            sActive.blink();
                        }
                        break;
                    case T_ON: // Trigger is active
                        if (level <= fReleaseLevel) // Signal is in peak
                        {
                            nCounter    = nReleaseCounter;
                            nState      = T_RELEASE;
                        }
                        break;
                    case T_RELEASE:
                        if (level > fReleaseLevel)
                            nState      = T_ON;
                        else if ((nCounter--) <= 0)
                        {
                            trigger_off(i, 0.0f);
                            nState      = T_OFF;
                            fVelocity   = 0.0f;
                        }
                        break;

                    default:
                        break;
                }

                // Log the velocity value
                sVelocity.process(fVelocity);
                if (fVelocity > max_velocity)
                    max_velocity        = fVelocity;
            }

            // Output meter value
            if (pActive != NULL)
                pActive->set_value(sActive.process(samples));

            pFunctionLevel->set_value(max_level);
            pVelocityLevel->set_value(max_velocity);
        }


        size_t trigger::decode_mode()
        {
            if (pMode == NULL)
                return dspu::SCM_PEAK;

            switch (size_t(pMode->value()))
            {
                case M_PEAK:    return dspu::SCM_PEAK;
                case M_RMS:     return dspu::SCM_RMS;
                case M_LPF:     return dspu::SCM_LPF;
                case M_UNIFORM: return dspu::SCM_UNIFORM;
                default:        break;
            }
            return dspu::SCM_PEAK;
        }

        size_t trigger::decode_source()
        {
            if (pSource == NULL)
                return dspu::SCS_MIDDLE;

            switch (size_t(pSource->value()))
            {
                case S_MIDDLE:  return dspu::SCS_MIDDLE;
                case S_SIDE:    return dspu::SCS_SIDE;
                case S_LEFT:    return dspu::SCS_LEFT;
                case S_RIGHT:   return dspu::SCS_RIGHT;
                default:        break;
            }
            return dspu::SCS_MIDDLE;
        }

        void trigger::update_settings()
        {
            dspu::filter_params_t fp;

            // Update settings for notes
            if (bMidiPorts)
            {
                nNote       = (pOctave->value() * 12) + pNote->value();
                lsp_trace("trigger note=%d", int(nNote));
            }

            // Update sidechain settings
            sSidechain.set_source(decode_source());
            sSidechain.set_mode(decode_mode());
            sSidechain.set_reactivity(pReactivity->value());
            sSidechain.set_gain(pPreamp->value());

            // Setup hi-pass filter for sidechain
            size_t hp_slope = pScHpfMode->value() * 2;
            fp.nType        = (hp_slope > 0) ? dspu::FLT_BT_BWC_HIPASS : dspu::FLT_NONE;
            fp.fFreq        = pScHpfFreq->value();
            fp.fFreq2       = fp.fFreq;
            fp.fGain        = 1.0f;
            fp.nSlope       = hp_slope;
            fp.fQuality     = 0.0f;
            sScEq.set_params(0, &fp);

            // Setup low-pass filter for sidechain
            size_t lp_slope = pScLpfMode->value() * 2;
            fp.nType        = (lp_slope > 0) ? dspu::FLT_BT_BWC_LOPASS : dspu::FLT_NONE;
            fp.fFreq        = pScLpfFreq->value();
            fp.fFreq2       = fp.fFreq;
            fp.fGain        = 1.0f;
            fp.nSlope       = lp_slope;
            fp.fQuality     = 0.0f;
            sScEq.set_params(1, &fp);

            // Update trigger settings
            fDetectLevel    = pDetectLevel->value();
            fDetectTime     = pDetectTime->value();
            fReleaseLevel   = fDetectLevel * pReleaseLevel->value();
            fReleaseTime    = pReleaseTime->value();
            fDynamics       = pDynamics->value() * 0.01f; // Percents
            fDynaTop        = pDynaRange1->value();
            fDynaBottom     = pDynaRange2->value();

            float out_gain  = pGain->value();
            fDry            = pDry->value() * out_gain;
            fWet            = pWet->value() * out_gain;
            bFunctionActive = pFunctionActive->value() >= 0.5f;
            bVelocityActive = pVelocityActive->value() >= 0.5f;

            // Update dynamics
            if (fDynaTop < 1e-6f)
                fDynaTop    = 1e-6f;
            if (fDynaBottom < 1e-6f)
                fDynaBottom = 1e-6f;
            if (fDynaTop < fDynaBottom)
            {
                float tmp   = fDynaTop;
                fDynaTop    = fDynaBottom;
                fDynaBottom = tmp;
            }

            // Update sampler settings
            sKernel.update_settings();

            // Update bypass
            bool bypass     = pBypass->value() >= 0.5f;
            for (size_t i=0; i<nChannels; ++i)
            {
                if (vChannels[i].sBypass.set_bypass(bypass))
                    pWrapper->query_display_draw();
                vChannels[i].bVisible   = vChannels[i].pVisible->value() >= 0.5f;
            }

            // Update pause
            bPause          = pPause->value() >= 0.5f;
            bClear          = pClear->value() >= 0.5f;

            // Update counters
            update_counters();
        }

        void trigger::ui_activated()
        {
            bUISync = true;
            sKernel.sync_samples_with_ui();
        }

        void trigger::update_sample_rate(long sr)
        {
            // Calculate number of samples per dot for shift buffer and initialize buffers
            size_t samples_per_dot  = dspu::seconds_to_samples(sr,
                    meta::trigger_metadata::HISTORY_TIME / meta::trigger_metadata::HISTORY_MESH_SIZE
                );

            // Update sample rate for bypass
            for (size_t i=0; i<nChannels; ++i)
            {
                vChannels[i].sBypass.init(sr);
                vChannels[i].sGraph.init(meta::trigger_metadata::HISTORY_MESH_SIZE, samples_per_dot);
            }
            sFunction.init(meta::trigger_metadata::HISTORY_MESH_SIZE, samples_per_dot);
            sVelocity.init(meta::trigger_metadata::HISTORY_MESH_SIZE, samples_per_dot);

            // Update settings on all samplers
            sKernel.update_sample_rate(sr);

            // Update trigger buffer
            sSidechain.set_sample_rate(sr);
            sScEq.set_sample_rate(sr);

            // Update activity blink
            sActive.init(sr);

            // Update counters
            update_counters();
        }

        void trigger::trigger_on(size_t timestamp, float level)
        {
            lsp_trace("midi_out = %p", pMidiOut);
            if (pMidiOut != NULL)
            {
                // We need to emit the NoteOn event
                plug::midi_t *midi  = pMidiOut->buffer<plug::midi_t>();
                lsp_trace("midi buffer = %p", midi);
                if (midi != NULL)
                {
                    // Create event
                    midi::event_t ev;
                    ev.timestamp    = timestamp;
                    ev.type         = midi::MIDI_MSG_NOTE_ON;
                    ev.channel      = nChannel;
                    ev.note.pitch   = nNote;
                    ev.note.velocity= uint32_t(1 + (level * 126));

                    // Store event in MIDI buffer
                    midi->push(ev);
                }
            }

            // Handle Note On event
            sKernel.trigger_on(timestamp, level);
        }

        void trigger::trigger_off(size_t timestamp, float level)
        {
            if (pMidiOut != NULL)
            {
                // We need to emit the NoteOff event
                plug::midi_t *midi  = pMidiOut->buffer<plug::midi_t>();
                if (midi != NULL)
                {
                    // Create event
                    midi::event_t ev;
                    ev.timestamp    = timestamp;
                    ev.type         = midi::MIDI_MSG_NOTE_OFF;
                    ev.channel      = nChannel;
                    ev.note.pitch   = nNote;
                    ev.note.velocity= 0;                        // Velocity is zero now

                    // Store event in MIDI buffer
                    midi->push(ev);
                }
            }

            // Do ont handle Note Off event by sampler because it will cause it to stop sample playback
            // sKernel.trigger_off(timestamp, level);
        }

        void trigger::process(size_t samples)
        {
            // Bypass MIDI events (additionally to the triggered events)
            if ((pMidiIn != NULL) && (pMidiOut != NULL))
            {
                plug::midi_t *in    = pMidiIn->buffer<plug::midi_t>();
                plug::midi_t *out   = pMidiOut->buffer<plug::midi_t>();

                // Bypass MIDI events from input to output
                if ((in != NULL) && (out != NULL))
                    out->push_all(in);

                // Output midi note number
                if (pMidiNote != NULL)
                    pMidiNote->set_value(nNote);
            }

            // Get pointers to channel buffers
            const float *ins[meta::trigger_metadata::TRACKS_MAX];
            float *outs[meta::trigger_metadata::TRACKS_MAX];
            float *ctls[meta::trigger_metadata::TRACKS_MAX];
            float preamp        = sSidechain.get_gain();

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c        = &vChannels[i];
                ins[i]              = (c->pIn != NULL)  ? c->pIn->buffer<float>() : NULL;
                outs[i]             = (c->pOut != NULL) ? c->pOut->buffer<float>() : NULL;

                // Update meter
                if ((ins[i] != NULL) && (c->pMeter != NULL))
                {
                    float level = (c->bVisible) ? dsp::abs_max(ins[i], samples) * preamp : 0.0f;
                    c->pMeter->set_value(level);
                }
            }
            pReleaseValue->set_value(fReleaseLevel);

            // Process samples
            for (size_t offset = 0; offset < samples; )
            {
                // Calculate amount of samples to process
                size_t to_process = samples - offset;
                if (to_process > meta::trigger_metadata::BUFFER_SIZE)
                    to_process  = meta::trigger_metadata::BUFFER_SIZE;

                // Prepare the control chain
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    ctls[i]             = c->vCtl;
                    dsp::mul_k3(ctls[i], ins[i], preamp, to_process);
                    c->sGraph.process(ctls[i], samples);
                }

                // Now we have to process data
                sSidechain.process(vTmp, ins, to_process);  // Pass input to sidechain
                process_samples(vTmp, to_process);          // Pass sidechain output for sample processing

                // Call sampler kernel for processing
                sKernel.process(ctls, NULL, to_process);

                // Now mix dry/wet signals and pass thru bypass switch
                for (size_t i=0; i<nChannels; ++i)
                {
                    dsp::mix2(ctls[i], ins[i], fWet, fDry, to_process);
                    vChannels[i].sBypass.process(outs[i], ins[i], ctls[i], to_process);
                }

                // Update pointers
                for (size_t i=0; i<nChannels; ++i)
                {
                    ins[i]         += to_process;
                    outs[i]        += to_process;
                }
                offset         += to_process;
            }

            if ((!bPause) || (bClear) || (bUISync))
            {
                // Process mesh requests
                for (size_t i=0; i<nChannels; ++i)
                {
                    // Get channel
                    channel_t *c        = &vChannels[i];
                    if (c->pGraph == NULL)
                        continue;

                    // Clear data if requested
                    if (bClear)
                        dsp::fill_zero(c->sGraph.data(), meta::trigger_metadata::HISTORY_MESH_SIZE);

                    // Get mesh
                    plug::mesh_t *mesh  = c->pGraph->buffer<plug::mesh_t>();
                    if ((mesh != NULL) && (mesh->isEmpty()))
                    {
                        // Fill mesh with new values
                        dsp::copy(mesh->pvData[0], vTimePoints, meta::trigger_metadata::HISTORY_MESH_SIZE);
                        dsp::copy(mesh->pvData[1], c->sGraph.data(), meta::trigger_metadata::HISTORY_MESH_SIZE);
                        mesh->data(2, meta::trigger_metadata::HISTORY_MESH_SIZE);
                    }
                }

                // Trigger function
                if (pFunction != NULL)
                {
                    // Clear data if requested
                    if (bClear)
                        dsp::fill_zero(sFunction.data(), meta::trigger_metadata::HISTORY_MESH_SIZE);

                    // Fill mesh if needed
                    plug::mesh_t *mesh = pFunction->buffer<plug::mesh_t>();
                    if ((mesh != NULL) && (mesh->isEmpty()))
                    {
                        dsp::copy(mesh->pvData[0], vTimePoints, meta::trigger_metadata::HISTORY_MESH_SIZE);
                        dsp::copy(mesh->pvData[1], sFunction.data(), meta::trigger_metadata::HISTORY_MESH_SIZE);
                        mesh->data(2, meta::trigger_metadata::HISTORY_MESH_SIZE);
                    }
                }

                // Trigger velocity
                if (pVelocity != NULL)
                {
                    // Clear data if requested
                    if (bClear)
                        dsp::fill_zero(sVelocity.data(), meta::trigger_metadata::HISTORY_MESH_SIZE);

                    // Fill mesh if needed
                    plug::mesh_t *mesh = pVelocity->buffer<plug::mesh_t>();
                    if ((mesh != NULL) && (mesh->isEmpty()))
                    {
                        dsp::copy(mesh->pvData[0], vTimePoints, meta::trigger_metadata::HISTORY_MESH_SIZE);
                        dsp::copy(mesh->pvData[1], sVelocity.data(), meta::trigger_metadata::HISTORY_MESH_SIZE);
                        mesh->data(2, meta::trigger_metadata::HISTORY_MESH_SIZE);
                    }
                }

                bUISync = false;
            }

            // Always query for draawing
            pWrapper->query_display_draw();
        }

        bool trigger::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > (M_RGOLD_RATIO * width))
                height  = M_RGOLD_RATIO * width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            // Calc axis params
            float zy    = 1.0f/GAIN_AMP_M_72_DB;
            float dx    = -float(width/meta::trigger_metadata::HISTORY_TIME);
            float dy    = height/(logf(GAIN_AMP_M_72_DB)-logf(GAIN_AMP_P_24_DB));

            // Draw axis
            cv->set_line_width(1.0);

            // Draw vertical lines
            cv->set_color_rgb(CV_YELLOW, 0.5f);
            for (float i=1.0; i < (meta::trigger_metadata::HISTORY_TIME-0.1f); i += 1.0f)
            {
                float ax = width + dx*i;
                cv->line(ax, 0, ax, height);
            }

            // Draw horizontal lines
            cv->set_color_rgb(CV_WHITE, 0.5f);
            for (float i=GAIN_AMP_M_48_DB; i<GAIN_AMP_P_24_DB; i *= GAIN_AMP_P_24_DB)
            {
                float ay = height + dy*(logf(i*zy));
                cv->line(0, ay, width, ay);
            }

            // Allocate buffer: t, f1(t), x, y
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            // Draw input signal
            static uint32_t c_colors[] = {
                    CV_MIDDLE_CHANNEL, CV_MIDDLE_CHANNEL,
                    CV_LEFT_CHANNEL, CV_RIGHT_CHANNEL
                   };
            bool bypass         = vChannels[0].sBypass.bypassing();
            float r             = meta::trigger_metadata::HISTORY_MESH_SIZE / float(width);

            for (size_t j=0; j<width; ++j)
            {
                size_t k        = r*j;
                b->v[0][j]      = vTimePoints[k];
            }

            cv->set_line_width(2.0f);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                if (!c->bVisible)
                    continue;

                // Initialize values
                float *ft       = c->sGraph.data();
                for (size_t j=0; j<width; ++j)
                    b->v[1][j]      = ft[size_t(r*j)];

                // Initialize coords
                dsp::fill(b->v[2], width, width);
                dsp::fill(b->v[3], height, width);
                dsp::fmadd_k3(b->v[2], b->v[0], dx, width);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypass) ? CV_SILVER : c_colors[(nChannels-1)*2 + i]);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            // Draw function (if present)
            if (bFunctionActive)
            {
                float *ft       = sFunction.data();
                for (size_t j=0; j<width; ++j)
                    b->v[1][j]      = ft[size_t(r*j)];

                // Initialize coords
                dsp::fill(b->v[2], width, width);
                dsp::fill(b->v[3], height, width);
                dsp::fmadd_k3(b->v[2], b->v[0], dx, width);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypass) ? CV_SILVER : CV_GREEN);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            // Draw events (if present)
            if (bVelocityActive)
            {
                float *ft       = sVelocity.data();
                for (size_t j=0; j<width; ++j)
                    b->v[1][j]      = ft[size_t(r*j)];

                // Initialize coords
                dsp::fill(b->v[2], width, width);
                dsp::fill(b->v[3], height, width);
                dsp::fmadd_k3(b->v[2], b->v[0], dx, width);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypass) ? CV_SILVER : CV_MEDIUM_GREEN);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            // Draw boundaries
            cv->set_color_rgb(CV_MAGENTA, 0.5f);
            cv->set_line_width(1.0);
            {
                float ay = height + dy*(logf(fDetectLevel*zy));
                cv->line(0, ay, width, ay);
                ay = height + dy*(logf(fReleaseLevel*zy));
                cv->line(0, ay, width, ay);
            }

            return true;
        }

        void trigger::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            v->write_object("sSidechain", &sSidechain);
            v->write_object("sScEq", &sScEq);
            v->write("vTmp", vTmp);

            v->write("nFiles", nFiles);
            v->write("nChannels", nChannels);
            v->write("bMidiPorts", bMidiPorts);

            v->write_object("sKernel", &sKernel);
            v->write_object("sFunction", &sFunction);
            v->write_object("sVelocity", &sVelocity);
            v->write_object("sActive", &sActive);

            v->begin_array("vChannels", &vChannels[0], meta::trigger_metadata::TRACKS_MAX);
            {
                for (size_t i=0; i< meta::trigger_metadata::TRACKS_MAX; ++i)
                {
                    const channel_t *c = &vChannels[i];
                    v->begin_object(c, sizeof(channel_t));
                    {
                        v->write("vCtl", c->vCtl);
                        v->write_object("sBypass", &c->sBypass);
                        v->write_object("sGraph", &c->sGraph);
                        v->write("bVisible", c->bVisible);

                        v->write("pIn", c->pIn);
                        v->write("pOut", c->pOut);
                        v->write("pGraph", c->pGraph);
                        v->write("pMeter", c->pMeter);
                        v->write("pVisible", c->pVisible);
                    }
                    v->end_object();
                }
            }
            v->end_array();
            v->write("vTimePoints", vTimePoints);

            v->write("nCounter", nCounter);
            v->write("nState", nState);
            v->write("fVelocity", fVelocity);
            v->write("bFunctionActive", bFunctionActive);
            v->write("bVelocityActive", bVelocityActive);

            v->write("nNote", nNote);
            v->write("nChannel", nChannel);
            v->write("fDry", fDry);
            v->write("fWet", fWet);
            v->write("bPause", bPause);
            v->write("bClear", bClear);
            v->write("bUISync", bUISync);

            v->write("nDetectCounter", nDetectCounter);
            v->write("nReleaseCounter", nReleaseCounter);
            v->write("fDetectLevel", fDetectLevel);
            v->write("fDetectTime", fDetectTime);
            v->write("fReleaseLevel", fReleaseLevel);
            v->write("fReleaseTime", fReleaseTime);
            v->write("fDynamics", fDynamics);
            v->write("fDynaTop", fDynaTop);
            v->write("fDynaBottom", fDynaBottom);
            v->write_object("pIDisplay", pIDisplay);

            v->write("pFunction", pFunction);
            v->write("pFunctionLevel", pFunctionLevel);
            v->write("pFunctionActive", pFunctionActive);
            v->write("pVelocity", pVelocity);
            v->write("pVelocityLevel", pVelocityLevel);
            v->write("pVelocityActive", pVelocityActive);
            v->write("pActive", pActive);

            v->write("pMidiIn", pMidiIn);
            v->write("pMidiOut", pMidiOut);
            v->write("pChannel", pChannel);
            v->write("pNote", pNote);
            v->write("pOctave", pOctave);
            v->write("pMidiNote", pMidiNote);

            v->write("pBypass", pBypass);
            v->write("pDry", pDry);
            v->write("pWet", pWet);
            v->write("pGain", pGain);
            v->write("pPause", pPause);
            v->write("pClear", pClear);
            v->write("pPreamp", pPreamp);
            v->write("pScHpfMode", pScHpfMode);
            v->write("pScHpfFreq", pScHpfFreq);
            v->write("pScLpfMode", pScLpfMode);
            v->write("pScLpfFreq", pScLpfFreq);

            v->write("pSource", pSource);
            v->write("pMode", pMode);
            v->write("pDetectLevel", pDetectLevel);
            v->write("pDetectTime", pDetectTime);
            v->write("pReleaseLevel", pReleaseLevel);
            v->write("pReleaseTime", pReleaseTime);
            v->write("pDynamics", pDynamics);
            v->write("pDynaRange1", pDynaRange1);
            v->write("pDynaRange2", pDynaRange2);
            v->write("pReactivity", pReactivity);
            v->write("pReleaseValue", pReleaseValue);
        }

    } /* namespace plugins */
} /* namespace lsp */

