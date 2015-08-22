;;; Calculate unsigned addition
;;; 
;;;   r3 = r0 + r1
;;; 
;;; where r0, r1, and r2 are clobbered.  The implementation does not do any
;;; overflow checking, so the result is actually
;;; 
;;;   r3 = min(r0 + r1, 1023)
;;;
;;; 
;;; The machine does only have unsigned subtraction, so we need to calculate
;;; the addition as
;;;
;;;   r3 = MAX_VAL - (MAX_VAL - r0 - r1);
;;; 
;;; But the subtraction is only done as decrement by 1, so this is expanded as
;;;
;;;   r2 = MAX_VAL;
;;;   while (r0--)
;;;     r2--;
;;;   while (r1--)
;;;     r2--;
;;;   r3 = MAX_VAL;
;;;   while (r2--)
;;;     r3--;

L0:	movdbz r2, 1024, L1, L1

L1:	movdbz r0, r0, L2, L3
L2:	movdbz r2, r2, L1, L1

L3:	movdbz r1, r1, L4, L5
L4:	movdbz r2, r2, L3, L3

L5:	movdbz r3, 1024, L7, L7

L7:	movdbz r2, r2, L8, exit
L8:	movdbz r3, r3, L7, L7
