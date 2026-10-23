"""Reproduce 8-group rigid camera-to-model calibration; no hardware access."""
import json
from pathlib import Path
import numpy as np

def rigid_fit(x,y):
    u,_,vt=np.linalg.svd((x-x.mean(0)).T@(y-y.mean(0)))
    r=vt.T@np.diag([1.,1.,np.linalg.det(vt.T@u.T)])@u.T
    return r,y.mean(0)-r@x.mean(0)

def build(d):
    samples=np.asarray(d['camera_samples'],dtype=float)
    y=np.asarray(d['robot_points'],dtype=float);x=samples.mean(axis=1)
    if samples.shape!=(8,3,3) or y.shape!=(8,3) or not np.isfinite(samples).all() or not np.isfinite(y).all():
        raise ValueError('Invalid calibration samples')
    if np.linalg.matrix_rank(x-x.mean(0))<3:raise ValueError('Degenerate 3D samples')
    r,t=rigid_fit(x,y);pred=x@r.T+t;errors=np.linalg.norm(pred-y,axis=1);loo=[]
    for i in range(len(x)):
        keep=np.arange(len(x))!=i;rr,tt=rigid_fit(x[keep],y[keep])
        loo.append(float(np.linalg.norm(rr@x[i]+tt-y[i])))
    return dict(coordinate_model='j5-reversed-v1',pairing_confirmed=True,
        calibration_id='20260929-downward-8groups',method='rigid_kabsch_equal_group_mean',
        camera_samples=samples.tolist(),robot_xyz=y.tolist(),camera_means=x.tolist(),
        R=r.tolist(),t=t.tolist(),predicted_robot_xyz=pred.tolist(),residual_mm=errors.tolist(),loo_mm=loo,
        rmse_mm=float(np.sqrt(np.mean(errors**2))),max_error_mm=float(errors.max()),
        loo_rmse_mm=float(np.sqrt(np.mean(np.array(loo)**2))),loo_max_mm=max(loo),
        camera_span_mm=np.ptp(samples,axis=1).tolist(),status='experimental_not_independently_validated',
        physical_point='User confirmed same tip/alignment point, all collected after arrival with downward attitude; robot_xyz are corrected model target coordinates. Fixed tool offset is absorbed only for this attitude and installation.',
        screenshot_times=d['screenshot_times'],robot_abc=[[0,180,0]]*8)

if __name__=='__main__':
    root=Path(__file__).resolve().parent
    d=json.loads((root/'samples_confirmed.json').read_text(encoding='utf-8'))
    result=build(d)
    (root/'camera_calibration.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('RMS',result['rmse_mm'],'max',result['max_error_mm'],'LOO RMS',result['loo_rmse_mm'])
