;; /etc/sherpk.d/hostname.scm
(use-modules (ice-9 rdelim))

(let ((host-file "/etc/HOSTNAME")
      (sysfs-file "/proc/sys/kernel/hostname"))
  
  (if (file-exists? host-file)
      (let* ((port (open-input-file host-file))
             (name (read-line port))) ; Read the first line
        (close-port port)
        
        (if (and name (not (string-null? name)))
            (begin
              (display (format #f "  ** Setting hostname to '~a' **\n" name))
              (let ((out (open-output-file sysfs-file)))
                (display name out)
                (close-port out)))
            (display "  !! /etc/HOSTNAME is empty !!\n")))
      
      (display "  !! /etc/HOSTNAME missing, skipping hostname setup !!\n")))
