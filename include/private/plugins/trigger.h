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

#ifndef PRIVATE_PLUGINS_TRIGGER_H_
#define PRIVATE_PLUGINS_TRIGGER_H_

#include <lsp-plug.in/plug-fw/plug.h>
#include <lsp-plug.in/plug-fw/core/IDBuffer.h>
#include <lsp-plug.in/dsp-units/ctl/Toggle.h>
#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/ctl/Blink.h>
#include <lsp-plug.in/dsp-units/util/MeterGraph.h>
#include <lsp-plug.in/dsp-units/util/Sidechain.h>
#include <lsp-plug.in/lltl/parray.h>
#include <lsp-plug.in/ipc/ITask.h>

#include <private/meta/trigger.h>
#include <private/plugins/trigger_kernel.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Trigger plugin
         */
        class trigger: public plug::Module
        {
            protected:
                enum state_t
                {
                    T_OFF,
                    T_DETECT,
                    T_ON,
                    T_RELEASE
                };

                enum source_t
                {
                    S_MIDDLE,
                    S_SIDE,
                    S_LEFT,
                    S_RIGHT
                };

                enum mode_t
                {
                    M_PEAK,
                    M_RMS,
                    M_LPF,
                    M_UNIFORM,
                };

                typedef struct channel_t
                {
                    float              *vCtl;           // Control chain
                    dspu::Bypass        sBypass;        // Bypass
                    dspu::MeterGraph    sGraph;         // Metering graph
                    bool                bVisible;       // Visibility flag

                    plug::IPort        *pIn;            // Input port
                    plug::IPort        *pOut;           // Output port
                    plug::IPort        *pGraph;         // Graph port
                    plug::IPort        *pMeter;         // Metering port
                    plug::IPort        *pVisible;       // Visibility port
                } channel_t;

            protected:
                // Sidechain
                dspu::Sidechain         sSidechain;             // Sidechain
                dspu::Equalizer         sScEq;                  // Sidechain equalizer
                float                  *vTmp;                   // Temporary buffer

                // Instantiation parameters
                size_t                  nFiles;                 // Number of files
                size_t                  nChannels;              // Number of channels
                bool                    bMidiPorts;             // Has MIDI port

                // Processors and buffers
                trigger_kernel          sKernel;                // Output kernel
                dspu::MeterGraph        sFunction;              // Function
                dspu::MeterGraph        sVelocity;              // Trigger velocity level
                dspu::Blink             sActive;                // Activity blink
                channel_t               vChannels[meta::trigger_metadata::TRACKS_MAX];  // Output channels
                float                  *vTimePoints;            // Time points buffer

                // Processing variables
                ssize_t                 nCounter;               // Counter for detect/release
                size_t                  nState;                 // Trigger state
                float                   fVelocity;              // Current velocity value
                bool                    bFunctionActive;        // Function activity
                bool                    bVelocityActive;        // Velocity activity

                // Parameters
                size_t                  nNote;                  // Trigger note
                size_t                  nChannel;               // Channel
                float                   fDry;                   // Dry amount
                float                   fWet;                   // Wet amount
                bool                    bPause;                 // Pause analysis refresh
                bool                    bClear;                 // Clear analysis
                bool                    bUISync;                // Synchronize with UI

                size_t                  nDetectCounter;         // Detect counter
                size_t                  nReleaseCounter;        // Release counter
                float                   fDetectLevel;           // Detection level
                float                   fDetectTime;            // Trigger detection time
                float                   fReleaseLevel;          // Release level
                float                   fReleaseTime;           // Release time
                float                   fDynamics;              // Dynamics
                float                   fDynaTop;               // Dynamics top
                float                   fDynaBottom;            // Dynamics bottom
                core::IDBuffer         *pIDisplay;              // Inline display buffer

                // Control ports
                plug::IPort            *pFunction;              // Trigger function
                plug::IPort            *pFunctionLevel;         // Function level
                plug::IPort            *pFunctionActive;        // Function activity
                plug::IPort            *pVelocity;              // Trigger velocity
                plug::IPort            *pVelocityLevel;         // Trigger velocity level
                plug::IPort            *pVelocityActive;        // Trigger velocity activity
                plug::IPort            *pActive;                // Trigger activity flag

                plug::IPort            *pMidiIn;                // MIDI input port
                plug::IPort            *pMidiOut;               // MIDI output port
                plug::IPort            *pChannel;               // Note port
                plug::IPort            *pNote;                  // Note port
                plug::IPort            *pOctave;                // Octave port
                plug::IPort            *pMidiNote;              // Output midi note #

                plug::IPort            *pBypass;                // Bypass port
                plug::IPort            *pDry;                   // Dry output
                plug::IPort            *pWet;                   // Wet output
                plug::IPort            *pGain;                  // Gain output
                plug::IPort            *pPause;                 // Pause analysis
                plug::IPort            *pClear;                 // Clear analysis
                plug::IPort            *pPreamp;                // Pre-amplification
                plug::IPort            *pScHpfMode;             // Sidechain high-pass filter mode
                plug::IPort            *pScHpfFreq;             // Sidechain high-pass filter frequency
                plug::IPort            *pScLpfMode;             // Sidechain low-pass filter mode
                plug::IPort            *pScLpfFreq;             // Sidechain low-pass filter frequency

                plug::IPort            *pSource;                // Source port
                plug::IPort            *pMode;                  // Mode port
                plug::IPort            *pDetectLevel;           // Detection level port
                plug::IPort            *pDetectTime;            // Detection time
                plug::IPort            *pReleaseLevel;          // Release level port
                plug::IPort            *pReleaseTime;           // Release time
                plug::IPort            *pDynamics;              // Dynamics
                plug::IPort            *pDynaRange1;            // Dynamics range 1
                plug::IPort            *pDynaRange2;            // Dynamics range 1
                plug::IPort            *pReactivity;            // Reactivity
                plug::IPort            *pReleaseValue;          // Release value

            protected:
                void                trigger_on(size_t timestamp, float level);
                void                trigger_off(size_t timestamp, float level);
                void                process_samples(const float *sc, size_t samples);
                inline void         update_counters();
                size_t              decode_mode();
                size_t              decode_source();

            public:
                explicit trigger(const meta::plugin_t *metadata, size_t channels, bool midi);
                virtual ~trigger();

            public:
                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports);
                virtual void        destroy();

                virtual void        update_settings();
                virtual void        update_sample_rate(long sr);
                virtual void        ui_activated();

                virtual void        process(size_t samples);
                virtual bool        inline_display(plug::ICanvas *cv, size_t width, size_t height);

                virtual void        dump(dspu::IStateDumper *v) const;
        };

    } // namespace plugins
} // namespace lsp

#endif /* PRIVATE_PLUGINS_TRIGGER_H_ */
