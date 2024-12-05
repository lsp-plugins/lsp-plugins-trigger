/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
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

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <lsp-plug.in/common/status.h>
#include <private/meta/trigger.h>

#define LSP_PLUGINS_TRIGGER_VERSION_MAJOR                   1
#define LSP_PLUGINS_TRIGGER_VERSION_MINOR                   0
#define LSP_PLUGINS_TRIGGER_VERSION_MICRO                   25

#define LSP_PLUGINS_TRIGGER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_TRIGGER_VERSION_MAJOR, \
        LSP_PLUGINS_TRIGGER_VERSION_MINOR, \
        LSP_PLUGINS_TRIGGER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        static const port_item_t trigger_sample_selectors[] =
        {
            { "0", "trigger.samp.0" },
            { "1", "trigger.samp.1" },
            { "2", "trigger.samp.2" },
            { "3", "trigger.samp.3" },
            { "4", "trigger.samp.4" },
            { "5", "trigger.samp.5" },
            { "6", "trigger.samp.6" },
            { "7", "trigger.samp.7" },
            { NULL, NULL }
        };

        static const port_item_t trigger_modes[] =
        {
            { "Peak",       "sidechain.peak"           },
            { "RMS",        "sidechain.rms"            },
            { "LPf",        "sidechain.lpf"            },
            { "SMA",        "sidechain.sma"            },
            { NULL, NULL }
        };

        static const port_item_t trigger_sources[] =
        {
            { "Middle",     "sidechain.middle" },
            { "Side",       "sidechain.side" },
            { "Left",       "sidechain.left" },
            { "Right",      "sidechain.right" },
            { NULL, NULL }
        };

        static const port_item_t trigger_areas[] =
        {
            { "Trigger",    "trigger.trig" },
            { "Instrument", "trigger.inst" },
            { NULL, NULL }
        };

        static const port_item_t trigger_filter_slope[] =
        {
            { "off",        "eq.slope.off"      },
            { "12 dB/oct",  "eq.slope.12dbo"    },
            { "24 dB/oct",  "eq.slope.24dbo"    },
            { "36 dB/oct",  "eq.slope.36dbo"    },
            { NULL, NULL }
        };

        //-------------------------------------------------------------------------
        // Trigger
        #define T_FILE_GAIN_MONO \
            AMP_GAIN10("mx", "Sample mix gain", 1.0f)
        #define T_FILE_GAIN_STEREO \
            PAN_CTL("pl", "Sample left channel panorama", -100.0f), \
            PAN_CTL("pr", "Sample right channel panorama", 100.0f)

        #define T_SAMPLE_FILE(gain)         \
            PATH("sf", "Sample file"),      \
            CONTROL("pi", "Sample pitch", U_SEMITONES, trigger_metadata::SAMPLE_PITCH), \
            CONTROL("hc", "Sample head cut", U_MSEC, trigger_metadata::SAMPLE_LENGTH), \
            CONTROL("tc", "Sample tail cut", U_MSEC, trigger_metadata::SAMPLE_LENGTH), \
            CONTROL("fi", "Sample fade in", U_MSEC, trigger_metadata::SAMPLE_LENGTH), \
            CONTROL("fo", "Sample fade out", U_MSEC, trigger_metadata::SAMPLE_LENGTH), \
            AMP_GAIN10("mk", "Sample makeup gain", 1.0f), \
            { "vl", "Sample velocity max",  U_PERCENT, R_CONTROL, F_LOWER | F_UPPER | F_STEP | F_LOWERING, 0.0f, 100.0f, 0.0f, 0.25, NULL }, \
            CONTROL("pd", "Sample pre-delay", U_MSEC, trigger_metadata::PREDELAY), \
            SWITCH("on", "Sample enabled", 1.0f), \
            TRIGGER("ls", "Sample listen"), \
            TRIGGER("lc", "Sample listen stop"), \
            SWITCH("rs", "Sample reverse", 0.0f), \
            gain, \
            BLINK("ac", "Sample activity"), \
            BLINK("no", "Sample note on event"), \
            { "fl", "Sample length", U_MSEC, R_METER, F_LOWER | F_UPPER | F_STEP, \
                    trigger_metadata::SAMPLE_LENGTH_MIN, trigger_metadata::SAMPLE_LENGTH_MAX, 0, trigger_metadata::SAMPLE_LENGTH_STEP, NULL }, \
            STATUS("fs", "Sample load status"), \
            MESH("fd", "Sample file contents", trigger_metadata::TRACKS_MAX, trigger_metadata::MESH_SIZE)

        #define T_METERS_MONO                   \
            MESH("isg", "Input signal graph", trigger_metadata::TRACKS_MAX, trigger_metadata::HISTORY_MESH_SIZE + 2), \
            METER_GAIN20("ism", "Input signal meter"), \
            SWITCH("isv", "Input signal display", 1.0f)

        #define T_METERS_STEREO                 \
            COMBO("ssrc", "Signal source", 0, trigger_sources), \
            MESH("isgl", "Input signal graph left", trigger_metadata::TRACKS_MAX, trigger_metadata::HISTORY_MESH_SIZE + 2), \
            MESH("isgr", "Input signal graph right", trigger_metadata::TRACKS_MAX, trigger_metadata::HISTORY_MESH_SIZE + 2), \
            METER_GAIN20("isml", "Input signal meter left"), \
            METER_GAIN20("ismr", "Input signal meter right"), \
            SWITCH("isvl", "Input signal left display", 1.0f), \
            SWITCH("isvr", "Input signal right display", 1.0f)

        #define T_PORTS_GLOBAL(sample)  \
            COMBO("asel", "Area selector", 0, trigger_areas), \
            BYPASS,                 \
            DRY_GAIN(1.0f),         \
            WET_GAIN(1.0f),         \
            PERCENTS("drywet", "Dry/Wet balance", 100.0f, 0.1f), \
            OUT_GAIN, \
            COMBO("mode", "Detection mode", trigger_metadata::MODE_DFL, trigger_modes), \
            SWITCH("pause", "Pause graph analysis", 0.0f), \
            TRIGGER("clear", "Clear graph analysis"), \
            AMP_GAIN100("preamp", "Signal pre-amplification", 1.0f), \
            COMBO("shpm", "High-pass filter mode", 0, trigger_filter_slope),      \
            LOG_CONTROL("shpf", "High-pass filter frequency", U_HZ, trigger_metadata::HPF),   \
            COMBO("slpm", "Low-pass filter mode", 0, trigger_filter_slope),      \
            LOG_CONTROL("slpf", "Low-pass filter frequency", U_HZ, trigger_metadata::LPF), \
            AMP_GAIN10("dl", "Detect level", trigger_metadata::DETECT_LEVEL_DFL), \
            CONTROL("dt", "Detect time", U_MSEC, trigger_metadata::DETECT_TIME), \
            AMP_GAIN1("rrl", "Relative release level", trigger_metadata::RELEASE_LEVEL_DFL), \
            CONTROL("rt", "Release time", U_MSEC, trigger_metadata::RELEASE_TIME), \
            CONTROL("dyna", "Dynamics", U_PERCENT, trigger_metadata::DYNAMICS), \
            AMP_GAIN("dtr1", "Dynamics range 1", GAIN_AMP_P_6_DB, 20.0f), \
            AMP_GAIN("dtr2", "Dynamics range 2", GAIN_AMP_M_36_DB, 20.0f), \
            CONTROL("react", "Reactivity", U_MSEC, trigger_metadata::REACTIVITY), \
            METER_OUT_GAIN("rl", "Release level", 20.0f), \
            MESH("tfg", "Trigger function graph", trigger_metadata::TRACKS_MAX, trigger_metadata::HISTORY_MESH_SIZE), \
            METER_GAIN20("tfm", "Trigger function meter"), \
            SWITCH("tfv", "Trigger function display", 1.0f), \
            BLINK("tla", "Trigger activity"), \
            MESH("tlg", "Trigger level graph", trigger_metadata::TRACKS_MAX, trigger_metadata::HISTORY_MESH_SIZE + 4), \
            METER_GAIN20("tlm", "Trigger level meter"), \
            SWITCH("tlv", "Trigger level display", 1.0f), \
            PORT_SET("ssel", "Sample selector", trigger_sample_selectors, sample)

        #define T_MIDI_PORTS                    \
            COMBO("chan", "Channel", trigger_metadata::MIDI_CHANNEL_DFL, midi_channels), \
            COMBO("note", "Note", trigger_metadata::MIDI_NOTE_DFL, notes), \
            COMBO("oct", "Octave", trigger_metadata::MIDI_OCTAVE_DFL, octaves), \
            { "mn", "MIDI Note #", U_NONE, R_METER, F_LOWER | F_UPPER | F_INT, 0, 127, 0, 0, NULL }

        static const port_t sample_file_mono_ports[] =
        {
            T_SAMPLE_FILE(T_FILE_GAIN_MONO),
            PORTS_END
        };

        static const port_t sample_file_stereo_ports[] =
        {
            T_SAMPLE_FILE(T_FILE_GAIN_STEREO),
            PORTS_END
        };

        static const port_t trigger_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            T_METERS_MONO,
            T_PORTS_GLOBAL(sample_file_mono_ports),

            PORTS_END
        };

        static const port_t trigger_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            T_METERS_STEREO,
            T_PORTS_GLOBAL(sample_file_stereo_ports),

            PORTS_END
        };

        static const port_t trigger_mono_midi_ports[] =
        {
            PORTS_MONO_PLUGIN,
            T_METERS_MONO,
            PORTS_MIDI_CHANNEL,
            T_MIDI_PORTS,
            T_PORTS_GLOBAL(sample_file_mono_ports),

            PORTS_END
        };

        static const port_t trigger_stereo_midi_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            T_METERS_STEREO,
            PORTS_MIDI_CHANNEL,
            T_MIDI_PORTS,
            T_PORTS_GLOBAL(sample_file_stereo_ports),

            PORTS_END
        };

        static const int plugin_classes[]           = { C_DYNAMICS, -1 };
        static const int clap_features_mono[]       = { CF_AUDIO_EFFECT, CF_UTILITY, CF_MONO, -1 };
        static const int clap_features_stereo[]     = { CF_AUDIO_EFFECT, CF_UTILITY, CF_STEREO, -1 };

        const meta::bundle_t trigger_bundle =
        {
            "trigger",
            "Trigger",
            B_UTILITIES,
            "mkCHORwcZcU",
            "This plugin implements trigger with mono input and mono output. Additional\nMIDI output is provided to pass notes generated by the trigger. There are\nup to eight samples available to play for different note velocities."
        };

        //-------------------------------------------------------------------------
        // Define plugin metadata
        const plugin_t trigger_mono =
        {
            "Triggersensor Mono",
            "Trigger Mono",
            "Trigger Mono",
            "TS1M",
            &developers::v_sadovnikov,
            "trigger_mono",
            {
                LSP_LV2_URI("trigger_mono"),
                LSP_LV2UI_URI("trigger_mono"),
                "zghv",
                LSP_VST3_UID("ts1m    zghv"),
                LSP_VST3UI_UID("ts1m    zghv"),
                0,
                NULL,
                LSP_CLAP_URI("trigger_mono"),
                LSP_GST_UID("trigger_mono"),
            },
            LSP_PLUGINS_TRIGGER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE | E_FILE_PREVIEW,
            trigger_mono_ports,
            "trigger/single/mono.xml",
            NULL,
            mono_plugin_port_groups,
            &trigger_bundle
        };

        const plugin_t trigger_stereo =
        {
            "Triggersensor Stereo",
            "Trigger Stereo",
            "Trigger Stereo",
            "TS1S",
            &developers::v_sadovnikov,
            "trigger_stereo",
            {
                LSP_LV2_URI("trigger_stereo"),
                LSP_LV2UI_URI("trigger_stereo"),
                "zika",
                LSP_VST3_UID("ts1s    zika"),
                LSP_VST3UI_UID("ts1s    zika"),
                0,
                NULL,
                LSP_CLAP_URI("trigger_stereo"),
                LSP_GST_UID("trigger_stereo"),
            },
            LSP_PLUGINS_TRIGGER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE | E_FILE_PREVIEW,
            trigger_stereo_ports,
            "trigger/single/stereo.xml",
            NULL,
            stereo_plugin_port_groups,
            &trigger_bundle
        };

        const plugin_t trigger_midi_mono =
        {
            "Triggersensor MIDI Mono",
            "Trigger MIDI Mono",
            "Trigger MIDI Mono",
            "TSM1M",
            &developers::v_sadovnikov,
            "trigger_midi_mono",
            {
                LSP_LV2_URI("trigger_midi_mono"),
                LSP_LV2UI_URI("trigger_midi_mono"),
                "t4yz",
                LSP_VST3_UID("tsm1m   t4yz"),
                LSP_VST3UI_UID("tsm1m   t4yz"),
                0,
                NULL,
                LSP_CLAP_URI("trigger_midi_mono"),
                LSP_GST_UID("trigger_midi_mono"),
            },
            LSP_PLUGINS_TRIGGER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE | E_FILE_PREVIEW,
            trigger_mono_midi_ports,
            "trigger/single/mono.xml",
            NULL,
            mono_plugin_port_groups,
            &trigger_bundle
        };

        const plugin_t trigger_midi_stereo =
        {
            "Triggersensor MIDI Stereo",
            "Trigger MIDI Stereo",
            "Trigger MIDI Stereo",
            "TSM1S",
            &developers::v_sadovnikov,
            "trigger_midi_stereo",
            {
                LSP_LV2_URI("trigger_midi_stereo"),
                LSP_LV2UI_URI("trigger_midi_stereo"),
                "9cqf",
                LSP_VST3_UID("tsm1s   9cqf"),
                LSP_VST3UI_UID("tsm1s   9cqf"),
                0,
                NULL,
                LSP_CLAP_URI("trigger_midi_stereo"),
                LSP_GST_UID("trigger_midi_stereo"),
            },
            LSP_PLUGINS_TRIGGER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE | E_FILE_PREVIEW,
            trigger_stereo_midi_ports,
            "trigger/single/stereo.xml",
            NULL,
            stereo_plugin_port_groups,
            &trigger_bundle
        };

    } /* namespace meta */
} /* namespace lsp */

