const fs = require('fs');
const path = require('path');
function widget(type, name, extra = '', children = '') {
  return `${type} ${name} {\n visible 1\n position 0 0\n size 100 30\n hexactpos 1\n vexactpos 1\n hexactsize 1\n vexactsize 1\n ${extra}\n${children ? ` {\n${children} }\n` : ''}}\n`;
}
function text(name, value = '', size = 18, color = '0.96 0.97 0.98 1') {
  return widget('TextWidgetClass', name, `ignorepointer 1\n color ${color}\n text "${value}"\n font "gui/fonts/sdf_MetronLight24"\n "exact text" 1\n "exact text size" ${size}\n "text halign" left\n "text valign" center\n wrap 0`);
}
function fill(name, color = '0.05 0.06 0.08 1') {
  return widget('ImageWidgetClass', name, `ignorepointer 1\n color ${color}`);
}
function button(name, label) {
  return widget('ButtonWidgetClass', name, 'text ""\n style Default\n color 1 1 1 1', fill(name + 'Fill') + fill(name + 'Accent') + text(name + 'Label', label));
}
let body = fill('Backdrop') + fill('Accent') + fill('Logo', '1 1 1 1');
body += text('Brand', 'COMMUNITY', 25) + text('Subtitle', 'DEATHMATCH', 16, '0.63 0.67 0.73 1');
body += text('Title', 'VOTE - NEXT ROUND', 30) + text('VoteTimer', '', 22);
body += fill('HeaderBackground') + text('ZoneHeader', 'ARENA') + text('PresetHeader', 'WEAPONS');
body += widget('FrameWidgetClass', 'ZoneViewport', 'ignorepointer 0');
body += widget('FrameWidgetClass', 'PresetViewport', 'ignorepointer 0');
body += text('ZoneRange', '', 15, '0.67 0.72 0.79 1') + text('PresetRange', '', 15, '0.67 0.72 0.79 1');
body += text('ZoneEmpty', 'Server chooses a random arena', 18) + text('PresetEmpty', 'Server chooses random weapons', 18);
for (const prefix of ['zbtn_', 'pbtn_']) {
  for (let i = 0; i < 8; i++) body += button(prefix + i, '');
  body += button(prefix + 'rand', 'RANDOM');
}
for (const column of ['Zone', 'Preset']) {
  body += button(column + 'Up', '^') + button(column + 'Track', '') + button(column + 'Down', 'v');
  body += fill(column + 'Thumb', '0.32 0.39 0.49 1');
}
body += fill('SelectionBackground') + text('SelectionText', 'YOUR VOTE', 16, '0.67 0.72 0.79 1');
body += text('ZonePick', '', 18) + text('PresetPick', '', 18);
body += text('Hint', 'Click a choice to vote. Click again to change.', 16, '0.67 0.72 0.79 1');
body += button('BtnClose', 'CLOSE');
body += fill('FooterLine', '0.16 0.19 0.23 1') + text('Footer', '', 15, '0.65 0.70 0.77 1');
body += fill('AttributionBackground', '0.025 0.03 0.04 0.94') + text('Attribution', 'Powered by Sentinel Deathmatch', 14, '0.65 0.70 0.77 1');
fs.writeFileSync(path.join(__dirname, '../layouts/dm_vote.layout'), widget('FrameWidgetClass', 'DmVoteRoot', 'clipchildren 1', body));
console.log('Generated Sentinel Deathmatch vote layout');
