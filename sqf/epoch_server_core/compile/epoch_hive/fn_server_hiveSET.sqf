/*
	Author: Aaron Clark - EpochMod.com

    Contributors: Florian Kinder

	Description:
	Hive SET 110 sync, 111 async

    Licence:
    Arma Public License Share Alike (APL-SA) - https://www.bistudio.com/community/licenses/arma-public-license-share-alike

    Github:
    https://github.com/EpochModTeam/Epoch/tree/release/Sources/epoch_server_core/compile/epoch_hive/fn_server_hiveSET.sqf
*/
params ["_prefix","_key","_value"];
_key = [_prefix,_key] joinString ":";
missionNamespace setVariable [["DB_",_key] joinString "_",[_value,-1]];
"epochserver" callExtension ([111,_key,"",_value] joinString "|")