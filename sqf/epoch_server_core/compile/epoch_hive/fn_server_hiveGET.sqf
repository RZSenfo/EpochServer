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

private ["_hiveResponse","_hiveStatus","_hiveMessage"];

//input will be overwritten
_hiveResponse = "epochserver" callExtension format (["200|%1:%2"] + _this);

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
