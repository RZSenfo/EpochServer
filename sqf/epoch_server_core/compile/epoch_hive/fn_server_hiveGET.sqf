/*
	Author: Aaron Clark - EpochMod.com

    Contributors:

	Description:
	Hive Get Data

    Licence:
    Arma Public License Share Alike (APL-SA) - https://www.bistudio.com/community/licenses/arma-public-license-share-alike

    Github:
    https://github.com/EpochModTeam/Epoch/tree/release/Sources/epoch_server_core/compile/epoch_hive/fn_server_hiveGET.sqf
*/

private _key = format (["%1:%2"] + _this);
private _ckey = ["DB",_key] joinString "_";
private _cache = missionNamespace getVariable _ckey;
if (isNil "_cache") exitWith {
	//input will be overwritten
	private _hiveResponse = "epochserver" callExtension format ["200|%1",_key];

	//the output is only filled with errors in this case
	if !(_hiveResponse isEqualTo "") then {
		
		#ifdef DEV_DEBUG
			diag_log "GET RETURN:";
			diag_log _hiveResponse;
		#endif

		_hiveResponse = parseSimpleArray _hiveResponse;
		_hiveResponse params [["_hiveStatus", 0],["_data",[]]];

		if (_hiveStatus isEqualTo 2) then {
			
			private _fetchKey = format ["290|%1",_data];
			private _hdata = "";

			private _loop = true;
			while {_loop} do {
				_hiveResponse = "epochserver" callExtension _fetchKey;
				_hdata = _hdata + _hiveResponse;
				_loop = count _hiveResponse == 10000;
			};

			parseSimpleArray _hdata params [["_realStatus",0],["_realData",[]]];
			_hiveStatus = _realStatus;
			_data = _realData;

			#ifdef DEV_DEBUG
				diag_log "GET2 RETURN:";
				diag_log _hdata;
			#endif

		};
		if (_hiveStatus == 1) then {
			missionNamespace setVariable [_ckey,[_data,-1]]
			[1,_data]
		} else {
			[0,[]]
		};
	} else {
		//something went wrong: output too large
		diag_log "GET RETURN:";
		diag_log [0, ["OUTPUT TOO LARGE"]];
		[0, ["OUTPUT TOO LARGE"]]
	};
};
_cache params ["_value",["_exp",-1]];
if (_exp < 0 || time < _exp) then {
	[1,_value]
} else {
	[0,[]]
};