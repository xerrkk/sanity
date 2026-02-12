;; /etc/sherpk.d/gpm.scm
(display "  ** Initializing Console Mouse Support (GPM) **\n")

;; -m /dev/input/mice: specifies the mouse device
;; -t imps2: specifies the protocol
;; -D: ensures it doesn't fork into the background (crucial for our supervisor!)
(register-service 'gpm 
                  '("/usr/sbin/gpm" "-m" "/dev/input/mice" "-t" "imps2" "-D") 
                  #t) ;; Respawn if it dies
