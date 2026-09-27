# Community UI branding

Sentinel Deathmatch lets a community control its leaderboard and voting
identity: the logo, community name, colors, subtitle, and promotional footer.
Copy [themes.json](examples/themes.json) to
`$profile:SentinelDeathmatch\themes.json` for a shared appearance. Optional
`leaderboard-theme.json` and `vote-ui-theme.json` files override individual
fields for their respective screens. Edit the files and restart the server.
See [UI themes](themes.md) for precedence and existing-file compatibility.

The independent `Powered by Sentinel Deathmatch` credit is required by
[LICENSE.md](../LICENSE.md). It is displayed separately from the footer and
cannot be replaced, hidden, or folded into other theme text.

## Custom logo companion add-on

A logo texture must be inside a local PBO. Do not put a URL in `LogoPath`, and
do not add assets to the Sentinel Deathmatch PBO. Create a separate add-on
from this example instead:

```text
CommunityBrand/
  $PBOPREFIX$
  config.cpp
  data/
    community_logo.paa
@CommunityBrand/
  addons/
    community_brand.pbo
    community_brand.pbo.community_brand.bisign
  keys/
    community_brand.bikey
```

The tracked example is in
[docs/examples/branding-addon](examples/branding-addon/). Its `config.cpp`
contains only a minimal `CfgPatches` declaration; it declares no Sentinel
Deathmatch dependency and contains no Sentinel Deathmatch files. Its prefix
is `CommunityBrand`, so the theme references the texture with:

```json
"LogoPath": "CommunityBrand\\data\\community_logo.paa"
```

### Make the `.paa`

Create an original **128 x 128 square PNG with transparency**. It is a good
fit for the leaderboard's 38 logical-pixel logo display. Run the DayZ Tools
converter from the image directory:

```powershell
& "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\ImageToPAA\ImageToPAA.exe" .\community_logo.png .\community_logo.paa
```

Put the result at `data\community_logo.paa`. Keep the source artwork and the
output path under your control; do not use another mod's texture without its
permission.

### Package, sign, and install

Keep `$PBOPREFIX$` at the PBO root, then use MakePbo to package the source
directory. Substitute your local tool locations if they differ:

```powershell
New-Item -ItemType Directory -Force .\@CommunityBrand\addons, .\@CommunityBrand\keys, .\keys
& "C:\Program Files (x86)\Mikero\DePboTools\bin\MakePbo.exe" .\CommunityBrand .\@CommunityBrand\addons\community_brand.pbo
Push-Location .\keys
& "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\DSUtils\DSCreateKey.exe" community_brand
Pop-Location
& "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\DSUtils\DSSignFile.exe" .\keys\community_brand.biprivatekey .\@CommunityBrand\addons\community_brand.pbo
Copy-Item .\keys\community_brand.bikey .\@CommunityBrand\keys\
Copy-Item .\keys\community_brand.bikey C:\dayz-server\keys\
```

Keep `community_brand.biprivatekey` private. Install the same
`@CommunityBrand` folder on the server and every client, and load it alongside
Sentinel Deathmatch. With signature verification enabled, the server needs
both the `.bisign` beside the companion PBO and the matching `.bikey` in its
`keys` folder. The core mod and texture-providing companion must both be
available to the server and clients.

If the logo does not render, check these in order:

1. The server was restarted after changing the JSON.
2. `LogoPath` exactly matches the PBO prefix and case of the packed texture.
3. The texture was converted to `.paa` and appears in the packaged PBO under
   `data\community_logo.paa`.
4. The companion add-on is loaded on both server and clients, and its signature
   key is installed when signature verification is enabled.
5. The JSON contains a local path only, with no URL or traversal sequence.

## Distribution boundary

You may distribute an original companion branding asset/config add-on under
the licensing exception, provided it does not copy, modify, repackage, bundle,
or redistribute Sentinel Deathmatch code or PBO contents. You may copy or
adapt the provided branding example templates solely for that companion
add-on, preserving their attribution. If you distribute a branding add-on
through Steam Workshop, its description must include
`Powered by Sentinel Deathmatch`.
