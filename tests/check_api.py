"""Usage: python3 tests/check_api.py /path/to/FusionFix/source/natives.ixx"""
import pathlib, re, sys
source=pathlib.Path(sys.argv[1]).read_text()
header=(pathlib.Path(__file__).resolve().parents[1]/'source/game.hpp').read_text()
names=['GET_PLAYER_ID','CONVERT_INT_TO_PLAYERINDEX','GET_PLAYER_CHAR','DOES_CHAR_EXIST','IS_CHAR_DEAD','IS_CHAR_IN_ANY_CAR','GET_CAR_CHAR_IS_USING','DOES_VEHICLE_EXIST','GET_CAR_SPEED','GET_CAR_COORDINATES','GET_CAR_HEADING','GET_CAR_HEALTH','GET_CAR_MODEL','IS_CAR_TYRE_BURST','IS_THIS_MODEL_A_BIKE','IS_THIS_MODEL_A_BOAT','IS_PAUSE_MENU_ACTIVE','IS_PLAYER_PLAYING','PRINT_STRING_WITH_LITERAL_STRING_NOW','IS_CHAR_SHOOTING']
line=' '.join(x for x in header.splitlines() if x.startswith('inline Native playerId') or x.startswith('inline Native shooting'))
actual=[int(x,16) for x in re.findall(r'\{(0x[0-9A-Fa-f]+)\}',line)]
expected=[int(re.search(r'\b'+n+r'\s*=\s*(0x[0-9a-fA-F]+)',source).group(1),16) for n in names]
assert actual==expected,(actual,expected)
print('PASS: all 20 native hashes match upstream FusionFix; runtime ABI still needs game validation')
