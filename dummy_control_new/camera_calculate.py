"""Read-only recalculation of the active calibration; no hardware access."""
import json
from pathlib import Path
import numpy as np


def fit(x,y):
    u,_,vt=np.linalg.svd((x-x.mean(0)).T@(y-y.mean(0)))
    d=np.eye(3);d[-1,-1]=np.linalg.det(vt.T@u.T)
    r=vt.T@d@u.T
    return r,y.mean(0)-r@x.mean(0)


if __name__=='__main__':
    path=Path(__file__).resolve().parent/'camera_calibration.json'
    data=json.loads(path.read_text(encoding='utf-8'))
    x=np.asarray(data['camera_samples'],float).mean(axis=1)
    y=np.asarray(data['robot_xyz'],float)
    r,t=fit(x,y)
    errors=np.linalg.norm(x@r.T+t-y,axis=1)
    print(json.dumps(dict(calibration_id=data.get('calibration_id'),R=r.tolist(),
        t=t.tolist(),residual_mm=errors.tolist(),
        rmse_mm=float(np.sqrt(np.mean(errors**2))),max_error_mm=float(errors.max()),
        stored_fit_matches=bool(np.allclose(r,data['R']) and np.allclose(t,data['t'])),
        note='No files changed. New fitting tool: calibration_20261012/fit_camera_calibration.py'),
        ensure_ascii=False,indent=2))
