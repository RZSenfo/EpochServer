/*
	Author: Aaron Clark - EpochMod.com

    Contributors:

	Description:
	Hive Get Getbit

    Licence:
    Arma Public License Share Alike (APL-SA) - https://www.bistudio.com/community/licenses/arma-public-license-share-alike

    Github:
    https://github.com/EpochModTeam/Epoch/tree/release/Sources/epoch_server_core/compile/epoch_hive/fn_server_hiveGETBIT.sqf
*/

private ["_hiveResponse","_hiveMessage"];
private _key = format (["%1:%2|%3"] + _this);
private _ckey = ["DB",_key] joinString "_";
private _cache = missionNamespace getVariable _ckey;
if (isNil "_cache") exitWith {
    _hiveMessage = false;
    _hiveResponse = "epochserver" callExtension format (["240|%1",_key]);
    if !(_hiveResponse isEqualTo "") then {
        _hiveResponse = parseSimpleArray _hiveResponse;
        _hiveResponse params [
            ["_status", 0],
            ["_data", "0"]
        ];
        if (_status == 1) then {
            _hiveMessage = (_data isEqualTo "1");
            missionNamespace setVariable [_ckey,_hiveMessage];
        };
    };
    _hiveMessage
};
_cache