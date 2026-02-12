(use-modules (ice-9 ftw)
             (ice-9 match))

(display "!! Director: Starting Boot Sequence !!\n")
(display (getcwd))

(define (init-hardware)
  ;; Use full paths for binaries since PATH might not be set yet
  (system* "/sbin/udevd" "--daemon")
  (system* "/sbin/udevadm" "trigger" "--action=add")
  (system* "/sbin/udevadm" "settle" "--timeout=10"))

(define (run-modules)
  (let ((dir-path "/etc/sherpa.d"))
    (if (and (file-exists? dir-path) 
             (eq? (stat:type (stat dir-path)) 'directory))
        (let ((files (scandir dir-path (lambda (f) (string-suffix? ".scm" f)))))
          (if (list? files)
              (for-each (lambda (f)
                          ;; Construct the absolute path
                          (let ((abs-path (string-append dir-path "/" f)))
                            (display (format #f "  ** Loading: ~a\n" abs-path))
                            (catch #t
                              (lambda () (primitive-load abs-path))
                              (lambda (key . args)
                                (display (format #f "  !! Failed to load ~a: ~a !!\n" f key))))))
                        files)
              (display "!! No .scm files found in /etc/sherpa.d !!\n")))
        (display "!! Error: /etc/sherpa.d is missing or not a directory !!\n"))))

(init-hardware)
(run-modules)
