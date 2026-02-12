(use-modules (ice-9 ftw)
             (ice-9 match))

(display "** Director scanning /etc/sherpk.d for modules... **\n")

;; Basic Hardware Init
(define (init-hardware)
  (system* "/sbin/udevd" "--daemon")
  (system* "/sbin/udevadm" "trigger" "--action=add")
  (system* "/sbin/udevadm" "settle"))

;; The module loader
(define (run-modules)
  (let ((dir "/etc/sherpk.d"))
    (if (file-exists? dir)
        (let ((scripts (scandir dir (lambda (f) (string-suffix? ".scm" f)))))
          (if scripts
              (for-each (lambda (f)
                          (display (string-append "  ** Loading " f " **\n"))
                          (load (string-append dir "/" f)))
                        scripts))))))

(init-hardware)
(run-modules)
(display "** All modules loaded. **\n")
