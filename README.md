# Anthem-style game prototype

A flying exosuit ("javelin") action game inspired by Anthem, built in **Unreal Engine 5** with C++.

**Current milestone: 1, flight feel.** You can walk, sprint, jump, hover and fly a placeholder Ranger suit around a huge **rainforest jungle**: rolling hills, cliffs, two rivers, lakes, waterfalls, rock tunnels, ruins, giant canopy trees and thousands of plants, all built from code. Flying builds **heat**; overheat and you drop out of the sky until the suit cools. Rivers, lakes, waterfalls and steep dives cool you down. You also carry an **assault rifle** with aiming, ammo, reloads and hit effects, and there's a shooting range next to the start. Your suit has **shields and armor**, and an **enemy camp** sends waves of ground soldiers and flying drones that chase you and shoot back. For bigger hits you have two pieces of Anthem-style **gear**, a frag grenade and a homing missile launcher, each on its own cooldown. Flight has real **momentum**, combat has **weight** (recoil, flinching, stagger, enemies that topple and fall out of the sky), and the suit feels **heavy** (footsteps, landing impacts, dust, camera shake and gamepad rumble).

The full design is in the project's UE5 implementation plan. Characters are placeholder shapes for now; art comes later.

---

## First-time setup (do this once)

### 1. Install the tools

1. **Epic Games Launcher**: download from <https://store.epicgames.com/download> and sign in.
2. **Unreal Engine 5**: in the launcher, go to **Unreal Engine → Library**, click the **+** next to *Engine Versions*, pick the newest 5.x version, and click **Install**. It is a large download (around 50 GB or more).
3. **Visual Studio 2022 Community** (free): <https://visualstudio.microsoft.com/downloads/>. In the installer, tick these workloads:
   - **Game development with C++** (on the right, also tick **Unreal Engine installer** and the newest **Windows 11 SDK**, or Windows 10 SDK if you're on Windows 10)
   - **Desktop development with C++**
4. **GitHub Desktop** (easiest way to get the code): <https://desktop.github.com/>. Sign in with your GitHub account.

### 2. Download the project

1. In GitHub Desktop, choose **File → Clone repository**.
2. Pick **jkolli2000-source/anthem-game** from the list (or paste `https://github.com/jkolli2000-source/anthem-game` in the **URL** tab).
3. Choose a folder with plenty of space and **no spaces or special characters in the path**, for example `C:\Dev\anthem-game`, and click **Clone**.
4. If you want to try a change before it is merged, use the **Current branch** button at the top of GitHub Desktop and pick that branch.

### 3. Point the project at your Unreal version

1. Open the project folder in File Explorer (in GitHub Desktop: **Repository → Show in Explorer**).
2. Right-click **AnthemGame.uproject** and choose **Switch Unreal Engine version...** (on Windows 11, click **Show more options** first to see it).
3. Pick the version you installed and click **OK**. This also creates the Visual Studio files.

### 4. Build and open the project

1. Double-click **AnthemGame.uproject**.
2. Unreal will say the **AnthemGame module is missing or built with a different engine version** and ask to rebuild it. Click **Yes**.
3. Wait. The first build takes a few minutes, and the editor's first launch also compiles shaders, which can take a while longer. That's normal.

**If the build fails:** open **AnthemGame.sln** (it's in the project folder), set the dropdowns at the top to **Development Editor** and **Win64**, then choose **Build → Build Solution**. Copy the errors from the **Error List** and send them over so they can be fixed.

### 5. Create the test level (once)

The project ships with code only, so you make the level in the editor:

1. **File → New Level → Basic → Create**. This gives you a sky, sun and floor.
2. **File → Save Current Level As...**, make a new folder called **Maps** inside **Content**, name the level **FlightTest**, and click **Save**.

From now on the editor opens this level automatically. (The first time, before it exists, you may see a "map not found" message; ignore it.)

You don't need to place anything. When you press Play, the game spawns your javelin and builds the jungle around you (see **The jungle** below). It takes a few seconds to build, so the first frame can be slow. The gray floor that comes with the Basic level is hidden automatically.

---

## Play and test

1. Press the green **Play** button in the toolbar (or **Alt+P**).
2. Click inside the game view so it captures your mouse. **Shift+F1** frees the mouse; **Esc** stops playing.

### Controls

| Action | Keyboard and mouse | Gamepad |
|---|---|---|
| Move | W A S D | Left stick |
| Look / steer | Mouse | Right stick |
| Jump (on ground) | Space | A |
| Hover (in the air), stop hovering | Space | A |
| Switch from flying to hovering | Space | A |
| Sprint (hold, on ground) | Left Shift | Click left stick |
| Start flying (in the air), stop flying | Left Shift | Click left stick |
| Take off straight into flight | F | Y |
| Flight: speed up / slow down | W / S | Left stick up / down |
| Fire (hold for full auto) | Left mouse | Right trigger |
| Aim down sights (hold) | Right mouse | Left trigger |
| Reload | R | X |
| Frag grenade | Q | Left bumper |
| Homing missiles | E | Right bumper |

You can shoot and use gear on the ground, while jumping or falling, and while hovering. Like in Anthem, weapons and gear are stowed while you fly: press **Space** to drop into a hover and shoot.

### The jungle

The world is about **4 km across** and is built entirely from code when you press Play, using Unreal's basic shapes. It's the same every time unless you change its **Random Seed**.

- **The start clearing** has the shooting range and the first enemy camp. Jungle surrounds it on every side.
- **Rivers:** two winding rivers cross the map, one to the north of the start and one to the west. Where they run through hills they cut canyons you can fly down. The lowlands have lakes and swamps.
- **Waterfalls:** three cliffs with water pouring off them into pools. Fly through the falling water to cool off fast.
- **Giant canopy trees:** about 30 huge trees, 90 to 150 m tall, with round branch platforms you can land on. Three of them stand around the start. Leaves have no collision, so you can fly straight through the canopy.
- **Ruins:** four ruined temples out in the jungle, each with its own enemy camp. The red **ENEMY CAMP** markers show where they are and how far. Camps farther from the start send bigger waves.
- **Rock tunnels:** two caves you can fly or walk through. The first is just behind the start camp, about 340 m south.
- **Mountains** ring the edge of the world, and stone spires stick up out of the forest.
- **Fog and light:** when play starts the level's fog becomes a thick, low jungle haze with light shafts, and the sun turns warm and golden. Fly higher to see farther.

**If it runs slowly:** place a **JungleWorld** in the level as described next, and lower its **Vegetation Density** (for example to 0.5) or **Tree Draw Distance**.

**To tweak the jungle:** drag a **JungleWorld** from the **Place Actors** panel (search "JungleWorld") into the level at 0, 0, 0. You'll see the whole jungle in the editor, and its **Details** panel under **Jungle** has the seed, world size, hill and mountain heights, water level, plant density, draw distances, and every color. It rebuilds after each change, which takes a few seconds. Save the level to keep it. To go back to the old graybox course, delete the JungleWorld and drag a **FlightTestArena** into the level instead. The game only builds the jungle by itself when the level has neither.

### Things to try

- **Take off and fly:** press **F**, then steer with the mouse. Hold **W** to boost and **S** to slow down.
- **Hover:** jump with **Space**, press **Space** again in the air, and look around. Hover heats you up more slowly than flying.
- **Overheat:** fly without stopping for about 11 seconds. The heat bar turns red, you drop, and you can't fly again until the bar is empty.
- **Cool down mid-flight:** dive steeply toward the ground, skim low over a river or lake, or fly through a waterfall. The nearest waterfall is about 350 m from the start, to the right and a little behind you as you face the shooting range; look for the cliff with white water down it. The bar shows **COOLING**.
- **Shooting range:** right in front of the start there are training dummies, dummies that strafe, and floating drones. Shoot the glowing orange head or eye for double damage (orange damage numbers). Destroyed targets come back after a few seconds.
- **Camera:** like Anthem, the camera sits over your right shoulder, so your suit stands a little left of center and the crosshair has a clear view. The crosshair turns **red** when it's over an enemy. In flight the camera swings in behind you and pulls far back.
- **Aim down sights:** hold the right mouse button to zoom over the shoulder and tighten your spread. Long bursts bloom the crosshair; let go briefly to recover accuracy.
- **Hover and shoot:** jump, press **Space** to hover, and shoot the drones in the air.
- **Frag grenade (Q):** your suit throws a grenade over its left shoulder toward the crosshair. It blows up when it hits anything and damages every enemy in the blast, which is about 10 m across and hits hardest in the middle. Throw it into a group of soldiers. It recharges in 8 seconds.
- **Homing missiles (E):** while the launcher is ready, blue corner brackets appear on up to four enemies near your crosshair. Press **E** and four missiles pop out of the pod on your left shoulder and curve in to hit them, even drones that are moving. With nothing bracketed they fly at the crosshair. They recharge in 12 seconds.
- **Cooldown icons:** the two squares to the right of the heat bar are your gear. While one recharges it fills up from the bottom and counts down; it lights up and flashes when it's ready. If you press the key too early (or while flying) the border flashes red.
- **Ammo:** a magazine holds 30 rounds and reloads automatically when empty (or press **R**). When your reserve runs out, walk up to the green crate with the yellow stripe to refill.
- **Fight the enemy camp:** turn around from the start and follow the red **ENEMY CAMP** marker (about 110 m away, behind and to the left). Soldiers walk toward you and fire bursts of red bolts; drones circle above you and fire big purple orbs. Their weak spots glow (soldiers: the visor and the power cell on the back; drones: the eye) and flare bright just before they fire, so watch for the flash and dodge. Clear a wave and a bigger one arrives after 12 seconds.
- **Shields and armor:** the blue bar (bottom left) is shields, the orange bar is armor. Hits take shields first; shields recharge on their own after 4 seconds without being hit, so duck behind a wall. Armor doesn't recharge: pick up the **green orbs** enemies drop. **White orbs** give ammo. Red arcs around the crosshair point toward whoever is shooting you.
- **Going down:** if your armor runs out the suit goes down and reboots back at the start after 5 seconds, with everything refilled.
- **Fly to escape:** soldiers can't follow you into the air, but drones can.
- **Land:** fly down onto the ground, a stone spire, or one of the round platforms in a giant canopy tree. Fly into a wall head-on at speed and you'll be knocked out of the air.

### How it feels (momentum and weight)

Everything still plays fast and arcade-y like Anthem, but now has weight behind it:

- **Flight momentum:** the suit turns toward where you look, but the faster you go the wider it turns. Yank the camera hard at full boost and you'll see the suit slide outward before it grips the air. Diving builds speed and that speed carries into the pull-up, so dive, then swoop back up. Climbing straight up slowly bleeds speed. Starting flight from a jump or fall keeps the momentum you had.
- **Heavy on foot:** the suit takes a moment to get up to speed and to stop, turns with some weight, and leans into speeding up, braking and turning. It jumps about as high as before but falls faster.
- **Footsteps:** every step bobs the camera a little. Sprinting stomps: dust kicks up at each foot and the gamepad thumps.
- **Landings:** the legs crouch to absorb the landing and the camera dips. Drop from high up, or fly straight into the ground, and you get a dust ring, a big shake, and a short moment where the suit is slowed while it gets its footing. Land out of fast flight and you skid along the ground.
- **Taking off** from the ground blasts a ring of dust. **Hovering** low blows dust off the ground under your jets, and flying fast and low leaves a dust wake.
- **Recoil:** holding the trigger the gun climbs harder and drifts a little to the right. Let go and the view settles most of the way back down by itself. Short bursts stay accurate. Each shot kicks your arms and the camera.
- **Accuracy:** shooting while running or in mid-air spreads your shots wider (watch the crosshair grow). Standing still, aiming, or hovering is steadiest.
- **Damage:** bullets lose some damage past about 40 m (down to 60% at 100 m). Enemy **armor plates** (the dark parts) take a bit less damage; the glowing **weak spots** still take double.
- **Hit reactions:** every hit makes enemies flinch away from it and pushes them back. Grenades and missiles throw them, and drones get tossed in the air. Enough damage at once **staggers** an enemy, and shooting its weak spot while it's flaring to fire also staggers it and **cancels the volley**.
- **Deaths:** soldiers topple over away from the killing shot, lie there for a couple of seconds and sink away. Drones lose power and drop, spinning and trailing black smoke, and blow up when they hit the ground.
- **Getting hit:** your camera jolts, the suit flinches and gets shoved a little. Hits that reach your armor, and shields breaking, hit harder. Explosions nearby shake the camera, and flying into a wall at speed throws sparks and dust.
- **Gamepad rumble** goes along with all of it. Turn it off under **Feel → Rumble** (see below).

### Tuning while you play

Every value is exposed in the editor. While playing, press **F8** to detach from the javelin, click it in the world (or select **JavelinCharacter** in the Outliner), and look in the **Details** panel:

- **Character Movement → Javelin | Flight / Hover / Ground / Landing** for speeds, thrust, drag, gravity in dives, turn rate at low and high speed, **Flight Grip** (lower = more drift in turns), dive and crash settings, and hard landing slowdown and skid. Stock settings like **Max Acceleration**, **Braking Deceleration Walking** and **Rotation Rate** set how heavy the suit feels on foot.
- **Feel** for camera shake (**Camera Shake Scale** 0 turns it off), flight buffeting, landing thresholds, dust color, how far hits shove you, and gamepad rumble on/off and strength.
- **Input → Gamepad** for right stick aiming: turn speed on foot and while aiming, how curved the response is (higher = slower small nudges), and how much and how quickly turning speeds up when you hold the stick to the side.
- **Heat** for heat gain and cooling rates.
- **Camera** for the camera profile used on foot, sprinting, hovering, flying and aiming. Socket Offset Y moves the camera sideways (bigger = suit further left), Arm Length is the distance behind you.
- **Weapon** for damage, range falloff, armor plate damage, fire rate, magazine size, reload time, spread (including moving and airborne spread), and recoil (kick, climb, drift and how much settles back).
- **Suit Health** for shields, armor and how fast shields recharge.
- **Gear** (on the **Abilities** component) for cooldowns, grenade damage and blast size, missile count, damage and lock-on range.
- To tune enemies, select one of them (they're listed as **EnemySoldier** and **EnemyDrone** in the Outliner while playing) and look under **Enemy**: health, speed, sight range, damage, fire rate, aim, and **Hit Reactions** (mass, how much damage staggers it, how far it flinches). The **EnemyEncounter** actor has the wave sizes.

Changes made while playing are lost when you stop. To keep a value, change its default in the C++ files under `Source/AnthemGame` (or ask for a change).

---

## How the code is organized

| File | What it does |
|---|---|
| `JavelinCharacter` | The player suit: camera, input bindings, mech suit body and gun, poses, flight tilt and banking, plus the feel: camera shake, landing crouch, footsteps, flinching, dust and rumble. |
| `JavelinMovementComponent` | Movement: walk, sprint, jump, plus custom **Flight** (with momentum and drift) and **Hover** modes, hard landings and skids. |
| `JavelinHeatComponent` | Heat meter: gain while flying or hovering, cooling, overheat lockout. |
| `CoolingVolume` | A box that cools the suit (water, waterfalls). Drag `CoolingVolume` into any level. |
| `JungleWorld` | The rainforest: terrain, rivers, lakes, waterfalls, caves, ruins, trees and plants, fog and light, water cooling, and the ruin camps. |
| `FlightTestArena` | The old graybox course, plus the shooting range and first enemy camp (the jungle uses just those). |
| `JavelinGameMode` | Spawns the javelin, HUD and jungle. |
| `JavelinWeaponComponent` | The gun: firing, ammo, reloading, aiming, spread, recoil, weak spot damage. |
| `DustEffect` | Dust and smoke puffs built from basic shapes: landings, footsteps, jets, crashes, smoke trails. |
| `ShotEffect` | Tracer, impact flash, puff and sparks for each shot, built from basic shapes. |
| `ShootingTarget` | Dummies and drones to shoot. Flash when hit, blow up, respawn. |
| `AmmoCrate` | Refills your ammo when you walk up to it. |
| `JavelinSuitHealthComponent` | Shields and armor: shields soak damage first and recharge, armor is repaired by orbs, the suit goes down at zero. |
| `JavelinEnemy` | Base for enemies: health, weak spots, spotting the player, chasing, firing volleys, dropping orbs. |
| `EnemySoldier` | Ground trooper that walks toward you, strafes and fires rifle bursts. |
| `EnemyDrone` | Flying drone that circles above you and fires slow, heavy orbs. |
| `EnemyProjectile` | The glowing bolts enemies fire. Visible and dodgeable. |
| `EnemyEncounter` | The enemy camp: spawns waves of soldiers and drones, bigger each time. |
| `PickupOrb` | Green (armor) and white (ammo) orbs dropped by enemies. |
| `JavelinAbilityComponent` | Gear: the frag grenade and homing missiles, their cooldowns, missile lock-on, and blast damage. |
| `JavelinGrenade` | The thrown grenade: arcs, blows up on impact. |
| `JavelinMissile` | One homing missile: pops out of the pod, speeds up and curves into its target. |
| `JavelinHUD` | Crosshair, hit markers, damage numbers, health bars, ammo, shields and armor, damage direction, heat bar, gear cooldown icons, missile lock brackets, wave status, speed and controls reminder. |

Input is created in code, so no input assets are needed yet. The plan's next milestone moves heat, flight and gear into the Gameplay Ability System.

## Saving your work to GitHub

Anything you create in the editor (like the FlightTest level) shows up in GitHub Desktop as a change. Write a short summary, click **Commit to ...**, then **Push origin**.
