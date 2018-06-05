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
params ["_prefix","_key"];

//input will be overwritten
private _input = format["200|%1:%2#%3", _prefix, _key, EPOCH_REALLYLONGDATABASESTRING];
_hiveResponse = "epochserver" callExtension _input;

//the output is only filled with errors in this case
if (_hiveResponse isEqualTo "") then {
    
    //Layout is xxxx#yyyyy where xxxx is the size of yyyyy

    //find the delimeter
    private _idx = _input find "#";

    //parse the size
    private _size = parseNumber (_input select [0, _idx - 1]);
    
    //select the actual output
    _input = _input select [_idx + 1, _size];

    // avoid parse if data is blank and return empty array
    if (_input isEqualTo "") exitWith {
        [0,[]]
    }; 
    
    parseSimpleArray _input

} else {
    //something went wrong: output too large
    [0, ["OUTPUT TOO LARGE"]]
};
