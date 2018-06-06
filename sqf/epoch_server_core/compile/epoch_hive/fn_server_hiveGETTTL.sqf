/*
	Author: Aaron Clark - EpochMod.com

    Contributors: Florian Kinder

	Description:
	Hive Get w/ TTL

    Licence:
    Arma Public License Share Alike (APL-SA) - https://www.bistudio.com/community/licenses/arma-public-license-share-alike

    Github:
    https://github.com/EpochModTeam/Epoch/tree/release/Sources/epoch_server_core/compile/epoch_hive/fn_server_hiveGETTTL.sqf
*/
private ["_hiveResponse","_hiveStatus","_hiveMessage","_whileCount"];

_hiveResponse = "epochserver" callExtension format (["210|%1:%2"] + _this);
if !(_hiveResponse isEqualTo "") then {
	_hiveResponse = parseSimpleArray _hiveResponse;

	_hiveResponse params [["_hiveStatus", 0],["_hiveTTL", -1],["_data",[]]];
	if (_hiveStatus == 1) then {

		[1,_data,_hiveTTL]
		
	} else {
		[0,[],-1]
	};

} else {
	[0,[],-1]
};
