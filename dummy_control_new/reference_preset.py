"""One source-derived preset, not a pose recovered from the user's photo.
Reference program: TCP (160,0,110), offset +160 along tool Z, RPY(0,180,0).
Thus controller endpoint (160,0,270). Normal XYZ paths remain uncompensated.
"""
from vertical_pose import down_branches,validate_down
from motion_settings import DEFAULT_SPEED

PRESET_XYZ=(160.,0.,270.)
PRESET_SPEED=DEFAULT_SPEED
# Pin the wrist branch instead of selecting a different posture based on current joints.
BRANCH_ANCHOR=(0.,3.271,86.367,0.,90.362,0.)

def preset_solution():
    valid=[]
    for q in down_branches(PRESET_XYZ):
        try:q,p=validate_down(PRESET_XYZ,q)
        except ValueError:continue
        if max(abs(a-b) for a,b in zip(q,BRANCH_ANCHOR))<=.1:valid.append((q,p))
    if len(valid)!=1:raise ValueError('参考下垂预设分支校验失败，未发送；请核对模型版本')
    return valid[0]
