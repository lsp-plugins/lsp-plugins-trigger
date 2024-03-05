<?php
	plugin_header();
	
	$midi   =   strpos($PAGE, '_midi_') > 0;
	$stereo =   strpos($PAGE, '_stereo') > 0;
	$sm     =   ($stereo) ? ' L, R' : '';
?>

<p>This plugin implements trigger with mono input and mono output.
<?php if ($midi) {?>
Additional MIDI output is provided to pass notes generated by the trigger.
<?php } ?>
There are up to eight samples available to play for different note velocities.</p>

<p><b>Controls:</b></p>
<ul>
	<li><b>Bypass</b> - hot bypass switch, when turned on (led indicator is shining), the plugin does not affect the input signal.</li>
	<li><b>Working area</b> - this control allows to switch the UI from trigger mode to instrument mode.</li>
	<li><b>Pause</b> - pauses any updates of the trigger graph.</li>
	<li><b>Clear</b> - clears all graphs.</li>
<?php if ($midi) {?>
	<li><b>Channel</b> - the MIDI channel to use for MIDI note.</li>
	<li><b>Note</b> - the note and the octave of the MIDI note generated by the trigger.</li>
	<li><b>MIDI #</b> - the MIDI number of the note. Allows to change the number with mouse scroll or mouse double click.</li>
<?php } ?>
	<li><b>In<?= $sm ?></b> - enables drawing of input signal graph and corresponding level meter.</li>
	<li><b>SC</b> - enables drawing of sidechain graph and corresponding level meter.</li>
	<li><b>Trg</b> - enables drawing of trigger signal graph and corresponding level meter.</li>
</ul>
<p><b>'Trigger' section:</b></p>
<ul>
	<li><b>Preamp</b> - input signal amplification for the sidechain.</li>
	<li><b>Mode</b> - combo box that allows to switch different modes for sidechain. Available modes are:</li>
	<ul>
		<li><b>Peak</b> - peak mode</li>
		<li><b>RMS</b> - root mean square of the input signal</li>
		<li><b>LPF</b> - input signal processed by one pole low-pass filter</li>
		<li><b>SMA</b> - input signal processed by SMA (Simple Moving Average) filter</li>
	</ul>
	<?php if ($stereo) { ?>
	<li><b>Source</b> - part of the input signal to use for sidechain processing:</li>
	<ul>
		<li><b>Middle</b> - middle part of signal is used for sidechain processing</li>
		<li><b>Side</b> - side part of signal is used for sidechain processing</li>
		<li><b>Left</b> - only left channel is used for sidechain processing</li>
		<li><b>Right</b> - only right channel is used for sidechain processing</li>
	</ul>
	<?php } ?>
	<li><b>LPF</b> - allows to set up slope and cut-off frequency for the low-pass filter applied to input signal.</li>
	<li><b>HPF</b> - allows to set up slope and cut-off frequency for the high-pass filter applied to input signal.</li>
	<li><b>Active</b> - trigger activity indicator.</li>
	<li><b>Reactivity</b> - the reactivity of the sidechain.</li>
	<li><b>Attack level</b> - the minimum level of the sidechain signal that forces trigger to trigger.</li>
	<li><b>Attack time</b> - the time gap used by the trigger to prevent false note-on detection.</li>
	<li><b>Release level</b> - the maximum level (relative to <b>Attack level</b>) of the sidechain signal that forces trigger to shut down.</li>
	<li><b>Release time</b> - the time gap used by the trigger to completely turn off after the signal becomes lower than <b>Release level</b>.</li>
	<li><b>Dynamics spread</b> - this knobs allows to add some dynamics to output signal.</li>
	<li><b>Dynamics range 1</b>, <b>Dynamics range 2</b> - the bounds of the range that allows to translate trigger level into velocity.</li>
</ul>
<p><b>'Samples' section:</b></p>
<ul>
	<li><b>Sample #</b> - the selector of the current displayable/editable sample.</li>
	<li><b>Head cut</b> - the time to be cut from the beginning of the current sample.</li>
	<li><b>Tail cut</b> - the time to be cut from the end of the current sample.</li>
	<li><b>Fade in</b> - the time to be faded from the beginning of the current sample.</li>
	<li><b>Fade out</b> - the time to be faded from the end of the current sample.</li>
	<li><b>Makeup</b> - the makeup gain of the sample volume.</li>
	<li><b>Pre-delay</b> - the time delay between the MIDI note has triggered and the start of the sample's playback</li>
	<li><b>Listen</b> - the button that forces the sample playback of the selected sample</li>
</ul>
<p><b>'Sample matrix' section:</b></p>
<ul>
	<li><b>Enabled</b> - enables/disables the playback of the corresponding sample.</li>
	<li><b>Active</b> - indicates that the sample is loaded, enabled and ready for playback.</li>
	<li><b>Velocity</b> - the maximum velocity of the note the sample can trigger. Allows to set up velocity layers between different samples.</li>
	<?php if ($stereo) { ?>
	<li><b>Pan Left</b> - the panorama of the left audio channel of the corresponding sample.</li>
	<li><b>Pan Right</b> - the panorama of the right audio channel of the corresponding sample.</li>
	<?php } else { ?>
	<li><b>Gain</b> - the additional gain adjust for the corresponding sample.</li>
	<?php } ?>
	<li><b>Listen</b> - the button that forces the playback of the corresponding sample.</li>
	<li><b>Note on</b> - indicates that the playback event of the correponding sample has triggered.</li>
</ul>
<p><b>'Audio channel' section:</b></p>
<ul>
	<li><b>Dry</b> - the gain of the input signal passed to the audio inputs of the plugin.</li>
	<li><b>Wet</b> - the gain of the processed signal.</li>
	<li><b>Dry/Wet</b> - the knob that controls the balance between the mixed dry and wet signal (see <b>Dry</b> and <b>Wet</b>) and the dry (unprocessed) signal.</li>
	<li><b>Output gain</b> - the overall output gain of the plugin.</li>
</ul>
