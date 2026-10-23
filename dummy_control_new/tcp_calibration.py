"""Pivot calibration: p_i + R_i tool = fixed point. No device commands."""
import json,os
from pathlib import Path
import numpy as np
from controller import numbers,rotation,angle_error

def fit_tcp(poses):
    if len(poses)<5:raise ValueError('至少采集5个不同朝向、对准同一固定点的样本')
    poses=[numbers(p,6) for p in poses]
    a=np.vstack([np.c_[rotation(p[3:]),-np.eye(3)] for p in poses]);b=-np.array([p[:3] for p in poses]).ravel()
    sol,_,rank,s=np.linalg.lstsq(a,b,rcond=None)
    if rank<6 or s[0]/s[-1]>100:raise ValueError('姿态变化不足或病态：需绕至少两个不同方向改变朝向，仍对准同一点')
    errors=np.linalg.norm((a@sol-b).reshape(-1,3),axis=1)
    rms=float(np.sqrt(np.mean(errors**2)))
    if np.linalg.norm(sol[:3])>500:raise ValueError('工具偏移超过500mm，请检查单位和同点采集')
    if rms>3 or max(errors)>5:raise ValueError(f'同点拟合误差过大：RMS {rms:.2f}mm，最大 {max(errors):.2f}mm；请重新对准')
    return dict(offset_mm=sol[:3].tolist(),pivot_base_mm=sol[3:].tolist(),rmse_mm=rms,max_error_mm=float(max(errors)),condition=float(s[0]/s[-1]),poses=poses)

def validation_error(result,pose):
    p=numbers(pose,6)
    return float(np.linalg.norm(np.array(p[:3])+np.array(rotation(p[3:]))@result['offset_mm']-result['pivot_base_mm']))

def check_holdout(result,pose):
    p=numbers(pose,6)
    if min(angle_error(p[3:],v[3:]) for v in result['poses'])<3:
        raise ValueError('独立验证请使用与拟合样本至少相差3度的新朝向，重新对准同一固定点')
    return validation_error(result,p)

def save_profile(path,result,down,validation,confirmed):
    if not confirmed:raise ValueError('请现场确认夹爪确实朝下，且验证点已对准同一固定点')
    # Always recompute fit and holdout error; never trust cached UI metrics.
    checked=fit_tcp(result['poses']);err=check_holdout(checked,validation)
    if err>5:raise ValueError(f'独立验证误差 {err:.2f}mm，超过5mm，不能保存为可用工具参数')
    down=numbers(down,6)
    d=dict(version=1,**checked,down_abc_deg=list(down[3:]),validation_pose=list(validation),validation_error_mm=err,
           definition='midpoint between gripper fingertips',physical_down_confirmed=True,
           note='Fixed mounting/opening and repeatable robot zero required; not camera calibration or collision validation.')
    path=Path(path);tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8');os.replace(tmp,path)
    return d

def load_profile(path):
    d=json.loads(Path(path).read_text(encoding='utf-8'))
    if d.get('version')!=1 or d.get('physical_down_confirmed') is not True:raise ValueError('工具参数未完成验证')
    numbers(d['offset_mm'],3);numbers(d['down_abc_deg'],3)
    check=fit_tcp(d['poses'])
    if not np.allclose(check['offset_mm'],d['offset_mm'],atol=1e-6) or check_holdout(check,d['validation_pose'])>5:raise ValueError('工具标定文件校验失败')
    return d
