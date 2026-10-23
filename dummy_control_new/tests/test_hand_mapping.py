import unittest
from mapped_controller import MappedController,map_joints
from kinematics import fk
from reference_motion import plan_grasp
from grasp_sequence import GraspSequence

class HandMappingTests(unittest.TestCase):
    def feedback(self,c,q,t):
        raw=map_joints(q)
        c.query=('joints',t);c.receive('ok '+' '.join(map(str,raw)),t+.01)
        c.query=('pose',t+.02);c.receive('ok '+' '.join(map(str,fk(raw))),t+.03)

    def make(self):
        sent=[];c=MappedController(sent.append,lambda _:None);c.enabled=True
        self.feedback(c,(0,-75,180,0,0,0),0)
        return c,sent

    def test_manual_mapping_and_ack(self):
        c,s=self.make();c.hand(True,.1);c.tick(.1)
        self.assertEqual(s[-1],'!HAND_C')
        c.receive('ok hand open',.15) # Wrong wire acknowledgement must not advance.
        self.assertEqual(c.hand_completed,0)
        c.receive('ok hand close',.2)
        self.assertEqual(c.last_hand_ack,'hand_open')
        c.hand(False,.3);c.tick(.3)
        self.assertEqual(s[-1],'!HAND_O')
        c.receive('ok hand open',.4)
        self.assertEqual(c.last_hand_ack,'hand_close')

    def test_full_physical_command_order(self):
        c,s=self.make();seq=GraspSequence(c,plan_grasp((160,0,215),c.joints),1)
        for i in range(1,150):
            t=i*.1
            self.feedback(c,c.joint_target if c.target is not None else c.joints,t)
            seq.tick(t+.04)
            if c.hand_queued:
                c.tick(t+.05)
                c.receive('ok hand close' if s[-1]=='!HAND_C' else 'ok hand open',t+.06)
            if seq.done:break
        self.assertTrue(seq.done)
        self.assertEqual(len(s),4)
        self.assertEqual(s[0],'!HAND_C')
        self.assertTrue(all(v.startswith('>') for v in s[1:3]))
        self.assertEqual(s[-1],'!HAND_O')

if __name__=='__main__':unittest.main()
