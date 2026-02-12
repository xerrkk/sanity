;; /etc/sherpk.d/dvorak.scm
(display "  ** Setting console layout to Dvorak **\n")

;; This assumes 'loadkeys' is available in /usr/bin or /bin
(let ((status (system* "loadkeys" "dvorak")))
  (if (not (zero? status))
      (display "  !! Failed to load dvorak keymap !!\n")))
