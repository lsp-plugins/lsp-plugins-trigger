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

#ifndef PRIVATE_META_TRIGGER_H_
#define PRIVATE_META_TRIGGER_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Sampler metadata
        struct trigger_metadata
        {
            static constexpr float SAMPLE_PITCH_MIN         = -24.0f;   // Minimum pitch (st)
            static constexpr float SAMPLE_PITCH_MAX         = 24.0f;    // Maximum pitch (st)
            static constexpr float SAMPLE_PITCH_DFL         = 0.0f;     // Pitch (st)
            static constexpr float SAMPLE_PITCH_STEP        = 0.01f;    // Pitch step (st)

            static constexpr float SAMPLE_LENGTH_MIN        = 0.0f;     // Minimum length (ms)
            static constexpr float SAMPLE_LENGTH_MAX        = 64000.0f; // Maximum sample length (ms)
            static constexpr float SAMPLE_LENGTH_DFL        = 0.0f;     // Sample length (ms)
            static constexpr float SAMPLE_LENGTH_STEP       = 0.1f;     // Sample step (ms)

            static constexpr float PREDELAY_MIN             = 0.0f;     // Pre-delay min (ms)
            static constexpr float PREDELAY_MAX             = 100.0f;   // Pre-delay max (ms)
            static constexpr float PREDELAY_DFL             = 0.0f;     // Pre-delay default (ms)
            static constexpr float PREDELAY_STEP            = 0.1f;     // Pre-delay step (ms)

            static constexpr size_t MESH_SIZE               = 320;      // Maximum mesh size
            static constexpr size_t TRACKS_MAX              = 2;        // Maximum tracks per mesh/sample
            static constexpr size_t SAMPLE_FILES            = 8;        // Number of sample files per trigger
            static constexpr size_t BUFFER_SIZE             = 4096;     // Size of temporary buffer
            static constexpr size_t PLAYBACKS_MAX           = 8192;     // Maximum number of simultaneously playing samples
            static constexpr float ACTIVITY_LIGHTING        = 0.1f;     // Activity lighting (seconds)

            static constexpr float  DETECT_LEVEL_DFL        = GAIN_AMP_M_12_DB;     // Default detection level [G]

            static constexpr float  RELEASE_LEVEL_MIN       = 0.0f;     // Minimum relative release level
            static constexpr float  RELEASE_LEVEL_DFL       = GAIN_AMP_M_3_DB;      // Default release level [G]
            static constexpr float  RELEASE_LEVEL_MAX       = 0.0f;     // Maximum relative release level
            static constexpr float  RELEASE_LEVEL_STEP      = 0.0001f;  // Release level step [G]

            static constexpr float  DETECT_TIME_MIN         = 0.0f;     // Minimum detection time [ms]
            static constexpr float  DETECT_TIME_DFL         = 5.0f;     // Default detection time [ms]
            static constexpr float  DETECT_TIME_MAX         = 20.0f;    // Maximum detection time [ms]
            static constexpr float  DETECT_TIME_STEP        = 0.0025f;  // Detection time step [ms]

            static constexpr float  RELEASE_TIME_MIN        = 0.0f;     // Minimum release time [ms]
            static constexpr float  RELEASE_TIME_DFL        = 10.0f;    // Default release time [ms]
            static constexpr float  RELEASE_TIME_MAX        = 100.0f;   // Maximum release time [ms]
            static constexpr float  RELEASE_TIME_STEP       = 0.005f;   // Release time step [ms]

            static constexpr float  DYNAMICS_MIN            = 0.0f;     // Minimum dynamics [%]
            static constexpr float  DYNAMICS_DFL            = 10.0f;    // Default dynamics [%]
            static constexpr float  DYNAMICS_MAX            = 100.0f;   // Maximum dynamics [%]
            static constexpr float  DYNAMICS_STEP           = 0.05f;    // Dynamics step [%]

            static constexpr float  REACTIVITY_MIN          = 0.000;    // Minimum reactivity [ms]
            static constexpr float  REACTIVITY_MAX          = 250;      // Maximum reactivity [ms]
            static constexpr float  REACTIVITY_DFL          = 20;       // Default reactivity [ms]
            static constexpr float  REACTIVITY_STEP         = 0.01;     // Reactivity step

            static constexpr float  HISTORY_TIME            = 5.0f;     // Amount of time to display history [s]
            static constexpr size_t HISTORY_MESH_SIZE       = 640;      // 640 dots for history

            static constexpr float  HPF_MIN                 = 10.0f;
            static constexpr float  HPF_MAX                 = 20000.0f;
            static constexpr float  HPF_DFL                 = 10.0f;
            static constexpr float  HPF_STEP                = 0.0025f;

            static constexpr float  LPF_MIN                 = 10.0f;
            static constexpr float  LPF_MAX                 = 20000.0f;
            static constexpr float  LPF_DFL                 = 20000.0f;
            static constexpr float  LPF_STEP                = 0.0025f;

            static constexpr float DRIFT_MIN                = 0.0f;     // Minimum delay
            static constexpr float DRIFT_DFL                = 0.0f;     // Default delay
            static constexpr float DRIFT_STEP               = 0.1f;     // Delay step
            static constexpr float DRIFT_MAX                = 100.0f;   // Maximum delay

            static constexpr float DYNA_MIN                 = 0.0f;     // Minimum dynamics
            static constexpr float DYNA_DFL                 = 0.0f;     // Default dynamics
            static constexpr float DYNA_STEP                = 0.05f;    // Dynamics step
            static constexpr float DYNA_MAX                 = 100.0f;   // Maximum dynamics

            static constexpr size_t MODE_DFL                = 1;        // RMS

            static constexpr size_t MIDI_CHANNEL_DFL        = 0;        // Default channel
            static constexpr size_t MIDI_NOTE_DFL           = 11;       // B
            static constexpr size_t MIDI_OCTAVE_DFL         = 2;        // 2nd octave
        };

        // Different samplers
        extern const plugin_t trigger_mono;
        extern const plugin_t trigger_stereo;
        extern const plugin_t trigger_midi_mono;
        extern const plugin_t trigger_midi_stereo;

    } // namespace meta
} // namespace lsp

#endif /* PRIVATE_META_TRIGGER_H_ */
