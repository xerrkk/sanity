;; /etc/sherpk.d/dhcpcd.scm

(display "  ** Configuring Networking (dhcpcd) **\n")

;; 1. Ensure loopback is up (standard practice)
(system* "/sbin/ip" "link" "set" "lo" "up")

;; 2. Register dhcpcd as a managed service.
;; We use "-B" (background/fork) or "-n" depending on how you want 
;; the supervisor to track it. For a simple supervisor, 
;; running in the foreground (-AB) is usually easier to track via PID.

(register-service 'networking 
                  '("/sbin/dhcpcd" "-AB" "--nobackground") 
                  #t) ;; #t means respawn if it crashes

(display "  ** dhcpcd registered and waiting for supervisor start **\n")
