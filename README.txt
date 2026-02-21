
This addon is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This addon is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this addon.  If not, see <http://www.gnu.org/licenses/>.

 This is the main addon Nasal hook. It MUST contain a function
 called "main". The main function will be called upon init with
 the addons.Addon instance corresponding to the addon being loaded.

 This script will live in its own Nasal namespace that gets
 dynamically created from the global addon init script.
 It will be something like "__addon[ADDON_ID]__" where ADDON_ID is
 the addon identifier, such as "org.flightgear.addons.Skeleton".

 See $FG_ROOT/Docs/README.add-ons for info about the addons.Addon
 object that is passed to main(), and much more. The latest version
 of this README.add-ons document is at:

   https://sourceforge.net/p/flightgear/fgdata/ci/next/tree/Docs/README.add-ons


Simple interface to edit the rain vectors in an aircraft.

Replace the following lines in rain-editor.xml with your aircraft's effects file splash vector properties.
There should be a set of x, y and z vectors for each window or group of windows.

setprop("/environment/aircraft-effects/splash-vector-left-side-x", xvector);
setprop("/environment/aircraft-effects/splash-vector-right-side-x", xvector);
setprop("/environment/aircraft-effects/splash-vector-back-x", xvector);

For example...

Replace "/environment/aircraft-effects/splash-vector-left-side-x" with your effect file property for the left window.

Also make sure you remove or comment out any autopilot, property rule or nasal commands that currently control these same
properties or it will interfear with the addon's ability to adjust the splash properties.
Once you get the values for the splash vectors dont forget to turn the autopilot, property rule or nasal commands back on.




