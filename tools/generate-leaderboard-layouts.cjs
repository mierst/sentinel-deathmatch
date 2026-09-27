const fs = require('fs');
const path = require('path');

const target = path.join(__dirname, '../layouts');
fs.mkdirSync(target, { recursive: true });

function widget(type, name, x, y, width, height, extra = '', children = '') {
  const childBlock = children ? ` {\n${children} }\n` : '';
  return `${type} ${name} {\n visible 1\n position ${x} ${y}\n size ${width} ${height}\n hexactpos 1\n vexactpos 1\n hexactsize 1\n vexactsize 1\n ${extra}\n${childBlock}}\n`;
}

function text(name, x, y, width, height, value, size = 20, align = 'left', color = '0.96 0.97 0.98 1') {
  return widget('TextWidgetClass', name, x, y, width, height, `ignorepointer 1\n color ${color}\n text "${value}"\n font "gui/fonts/sdf_MetronLight24"\n "exact text" 1\n "exact text size" ${size}\n "text halign" ${align}\n "text valign" center\n wrap 0`);
}

function panel(name, x, y, width, height, color, children = '') {
  return widget('ImageWidgetClass', name, x, y, width, height, `ignorepointer 1\n color ${color}`, children);
}

function button(name, x, y, width, height, value) {
  return widget('ButtonWidgetClass', name, x, y, width, height, 'text ""\n style Default\n color 1 1 1 1', text(name + 'Label', 0, 0, width, height, value, 18, 'center'));
}

let board = panel('Backdrop', 0, 0, 960, 830, '0.027 0.031 0.039 0.97');
board += panel('Accent', 0, 0, 960, 3, '0.23 0.51 0.96 1');
board += widget('ImageWidgetClass', 'Logo', 26, 24, 38, 38, 'ignorepointer 1\n color 1 1 1 1\n imageTexture ""\n mode blend\n blend 1\n sourcealpha 1');
board += text('Brand', 78, 22, 300, 38, 'COMMUNITY', 25);
board += text('Subtitle', 26, 65, 700, 25, 'DEATHMATCH', 16, 'left', '0.63 0.67 0.73 1');
board += text('Title', 26, 98, 650, 46, 'ROUND STANDINGS', 32);
board += text('Status', 650, 104, 282, 35, 'ROUND 1 / LIVE', 18, 'right', '0.70 0.75 0.82 1');
board += button('Round', 26, 158, 160, 36, 'THIS ROUND');
board += button('Session', 194, 158, 150, 36, 'SESSION');
board += panel('TabAccent', 26, 194, 160, 3, '0.23 0.51 0.96 1');
board += text('Count', 700, 158, 234, 36, '0 PLAYERS', 16, 'right', '0.65 0.70 0.76 1');
board += panel('HeaderBackground', 0, 214, 960, 36, '0.09 0.10 0.13 1');
board += widget('FrameWidgetClass', 'Header', 0, 214, 936, 36, 'ignorepointer 1\n clipchildren 1',
  text('H_Rank', 26, 0, 64, 36, 'RANK', 16) +
  text('H_Name', 100, 0, 432, 36, 'PLAYER', 16) +
  text('H_Kills', 558, 0, 88, 36, 'KILLS', 16, 'right') +
  text('H_Deaths', 658, 0, 88, 36, 'DEATHS', 16, 'right') +
  text('H_Streak', 758, 0, 130, 36, 'BEST STREAK', 16, 'right'));
board += widget('FrameWidgetClass', 'Rows', 0, 250, 936, 420, 'clipchildren 1');
board += text('Empty', 26, 350, 884, 48, 'LOADING STANDINGS...', 22, 'center');
board += button('ScrollUp', 936, 250, 24, 26, '^');
board += button('ScrollTrack', 936, 278, 24, 364, '');
board += panel('ScrollThumb', 940, 280, 16, 50, '0.32 0.39 0.49 1');
board += button('ScrollDown', 936, 644, 24, 26, 'v');
board += widget('FrameWidgetClass', 'Own', 0, 678, 936, 42, 'clipchildren 1');
board += text('Range', 26, 730, 450, 34, '0 - 0 OF 0', 16, 'left', '0.67 0.72 0.79 1');
board += button('Find', 590, 730, 140, 36, 'FIND ME');
board += button('Close', 746, 730, 184, 36, 'CLOSE');
board += panel('FooterLine', 26, 780, 908, 1, '0.16 0.19 0.23 1');
board += text('Footer', 26, 792, 610, 24, '', 15, 'left', '0.65 0.70 0.77 1');
board += panel('AttributionBackground', 638, 791, 296, 26, '0.025 0.03 0.04 0.94');
board += text('Attribution', 646, 792, 288, 24, 'Powered by Sentinel Deathmatch', 14, 'right', '0.65 0.70 0.77 1');
fs.writeFileSync(path.join(target, 'dm_scoreboard.layout'), widget('FrameWidgetClass', 'DmScoreboardRoot', 0, 0, 960, 830, 'clipchildren 1', board));

let row = panel('RowBackground', 0, 0, 936, 38, '0.06 0.07 0.09 1');
row += panel('RowAccent', 0, 0, 3, 38, '0.23 0.51 0.96 1');
row += text('Rank', 26, 0, 64, 38, '1', 19, 'left', '0.67 0.72 0.79 1');
row += text('Name', 100, 0, 432, 38, 'Survivor', 22);
row += text('Kills', 558, 0, 88, 38, '0', 22, 'right');
row += text('Deaths', 658, 0, 88, 38, '0', 22, 'right');
row += text('Streak', 758, 0, 130, 38, '0', 22, 'right');
row += panel('RowLine', 26, 37, 884, 1, '0.13 0.15 0.18 1');
fs.writeFileSync(path.join(target, 'dm_scoreboard_row.layout'), widget('FrameWidgetClass', 'DmScoreboardRow', 0, 0, 936, 38, 'ignorepointer 1\n clipchildren 1', row));

console.log('Generated Sentinel Deathmatch leaderboard layouts');
