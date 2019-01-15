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

private _key = format (["%1:%2"] + _this);
private _ckey = ["DB",_key] joinString "_";
private _cache = missionNamespace getVariable _ckey;
if (isNil "_cache") exitWith {

	private _hiveResponse = "epochserver" callExtension format ["210|%1",_key];

	if !(_hiveResponse isEqualTo "") then {

		#ifdef DEV_DEBUG    
			diag_log "GETTTL RETURN:";
			diag_log _hiveResponse;
		#endif

		_hiveResponse = parseSimpleArray _hiveResponse;
		_hiveResponse params [["_hiveStatus", 0],["_hiveTTL", -1],["_data",[]]];

		if (_hiveStatus == 2) then {
			
			private _fetchKey = format ["290|%1",_hiveTTL];
			private _hdata = "";

			private _loop = true;
			while {_loop} do {
				_hiveResponse = "epochserver" callExtension _fetchKey;
				_hdata = _hdata + _hiveResponse;
				_loop = count _hiveResponse == 10000;
			};

			parseSimpleArray _hdata params [["_realStatus",0],["_realTTL",-1],["_realData",[]]];
			_hiveStatus = _realStatus;
			_hiveTTL = _realTTL;
			_data = _realData;
			
		#ifdef DEV_DEBUG
				diag_log "GETTTL RETURN2:";
				diag_log _hdata;
		#endif

		};
		

		if (_hiveStatus == 1) then {
			missionNamespace setVariable [_ckey,[_data,time+_hiveTTL]]
			[1,_data,_hiveTTL]
		} else {
			[0,[],-1]
		};

	} else {
		//something went wrong
		[0, [], -1]
	};
};
_cache params ["_data",["_exp",-1]];

if (_exp < 0 || time < _exp) then {
	[1,_data,_exp]
} else {
	[0,[],-1]
};