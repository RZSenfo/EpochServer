/*
	Author: Aaron Clark - EpochMod.com

    Contributors:

	Description:
	Hive Expire 130 sync, 131 async

    Licence:
    Arma Public License Share Alike (APL-SA) - https://www.bistudio.com/community/licenses/arma-public-license-share-alike

    Github:
    https://github.com/EpochModTeam/Epoch/tree/release/Sources/epoch_server_core/compile/epoch_hive/fn_server_hiveEXPIRE.sqf
*/
params ["_prefix","_key","_expires"];
_key = format ["%1:%2",_prefix,_key];
private _ckey = ["DB",_key] joinString "_";
private _cache = missionNamespace getVariable _ckey;
if (!isNil "_cache") then {
    _cache set [1, _expires];
    missionNamespace setVariable [_ckey,_cache];
};
"epochserver" callExtension format ["131|%1|%2",_key,_expires];
