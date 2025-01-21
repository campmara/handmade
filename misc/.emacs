; Mara's Ever-growing .emacs config
; Much of this has used functions and ideas from Casey Muratori's .emacs config, with
; some additional settings for my own personal text editing preferences.

; Platform *****************************************************************************************

; Determine the current OS
(setq mara-aquamacs (featurep 'aquamacs))
(setq mara-linux (featurep 'x))
(setq mara-win32 (not (or mara-aquamacs mara-linux)))

; Set build-related platform files
(when mara-aquamacs
    (cua-mode 0) 
    (osx-key-mode 0)
    (tabbar-mode 0)
    (setq mac-command-modifier 'meta)
    (setq x-select-enable-clipboard t)
    (setq aquamacs-save-options-on-quit 0)
    (setq special-display-regexps nil)
    (setq special-display-buffer-names nil)
    (define-key function-key-map [return] [13])
    (setq mac-command-key-is-meta t)
    (scroll-bar-mode nil)
    (setq mac-pass-command-to-system nil)
    (setq mara-compilescript "./build.macosx")
)

(when mara-linux
    (setq mara-compilescript "./build.linux")
    (display-battery-mode 1)
)

(when mara-win32
    (setq mara-compilescript "build.bat")
)

; **************************************************************************************************

; Window config
;;(add-to-list 'initial-frame-alist '(fullscreen . maximized))
(setq initial-frame-alist '((top . 15) (left . 50) (width . 250) (height . 65)))

; Make emacs silent
(set-message-beep 'silent)

; Configure tabs (spaces only, 4 wide)
(setq-default indent-tabs-mode nil)
(setq-default tab-width 4)

; KEY REBINDS **************************************************************************************

(defun next-blank-line ()
    "Moves to the next line containing nothing but whitespace."
    (interactive)
    (forward-line)
    (search-forward-regexp "^[ \t]*\n")
    (forward-line -1)
)

(defun previous-blank-line ()
    "Moves to the previous line containing nothing but whitespace."
    (interactive)
    (search-backward-regexp "^[ \t]*\n")
)

; Arrow keys move forward, backward, up, down
(keymap-global-set "C-<right>" 'forward-word)
(keymap-global-set "C-<left>" 'backward-word)
(keymap-global-set "C-<up>" 'previous-blank-line)
(keymap-global-set "C-<down>" 'next-blank-line)

; Rebind search to C-x C-s and bind C-s to save
(keymap-global-set "C-x C-s" 'isearch-forward)
(keymap-global-set "C-s" 'save-buffer)

; Rebind undo to C-z and bind that silly "yeet the window" function to C-x u
; Sorry I'm not 40 years old guys!
(keymap-global-set "C-z" 'undo)
(keymap-global-set "C-S-z" 'undo-redo)
(keymap-global-set "C-x u" 'suspend-frame)

; Indentation rebinds
(defun mara-tab (distance)
    "Indents the region or line by 'distance' spaces. Can be negative to shift left."
    (interactive)
    (if (use-region-p)
        (let ((mark (mark)))
            (save-excursion
                (indent-rigidly (region-beginning) (region-end) distance)
                (push-mark mark t t)
                (setq deactivate-mark nil)))
        (indent-rigidly (line-beginning-position) (line-end-position) distance)
    )
)

(defun mara-tab-minus-four ()
    "Indents the region or line by 4 spaces to the right."
    (interactive)
    (mara-tab -4)
)

(keymap-global-set "<tab>" 'indent-for-tab-command)
(keymap-global-set "<backtab>" 'mara-tab-minus-four)
(keymap-global-set "C-<tab>" 'indent-region)

; **************************************************************************************************

; Compilation **************************************************************************************

; Keybinds
(defun mara-quick-compile ()
    "Compiles from the build script."
    (interactive)
    (compile mara-compilescript)
    (other-window 1)
)

(keymap-global-set "<f5>" 'mara-quick-compile)

; **************************************************************************************************

(custom-set-variables
    '(auto-save-default nil)
    '(auto-save-interval 0)
    '(auto-save-list-file-prefix nil)
    '(auto-save-timeout 0)
    '(delete-auto-save-files nil)
    '(make-backup-file-name-function (quote ignore))
    '(make-backup-files nil)
    '(mouse-wheel-follow-mouse nil)
    '(mouse-wheel-progressive-speed nil)
    '(mouse-wheel-scroll-amount '(0.07))
    '(version-control nil)
    '(whitespace-display-mappings
        '((space-mark 32 [183]
            [46])
            ;; (newline-mark 10 [36 10])
            ))
    '(whitespace-line-column 100)
    '(delete-selection-mode 1)
)

(custom-set-faces
    '(whitespace-space ((t (:foreground "#1b1c1f"))))
)

; C/C++ Style Definition ***************************************************************************

; Bright-red TODOs
(setq fixme-modes '(c++-mode c-mode emacs-lisp-mode))
(make-face 'font-lock-fixme-face)
(make-face 'font-lock-note-face)
(mapc (lambda (mode)
    (font-lock-add-keywords
    mode
    '(("\\<\\(TODO\\)" 1 'font-lock-fixme-face t)
        ("\\<\\(NOTE\\)" 1 'font-lock-note-face t))))
fixme-modes)
(modify-face 'font-lock-fixme-face "Red" nil nil t nil t nil nil)
(modify-face 'font-lock-note-face "Dark Green" nil nil t nil t nil nil)

(defconst mara-c-style
    '((c-tab-always-indent        . t)
      (c-comment-only-line-offset . 0)
      (c-offsets-alist            . ((arglist-close         .  c-lineup-arglist)
                                     (label                 . -4)
                                     (access-label          . -4)
                                     (substatement-open     .  0)
                                     (statement-case-intro  .  4)
                                     (statement-block-intro .  c-lineup-for)
                                     (case-label            .  4)
                                     (block-open            .  0)
                                     (inline-open           .  0)
                                     (topmost-intro-cont    .  0)
                                     (knr-argdecl-intro     . -4)
                                     (brace-list-open       .  0)
                                     (brace-list-intro      .  4))))
    "Mara's C/C++ Style"
)

(defun mara-c-save ()
    "Saves the buffer and removes all trailing whitespace."
    (interactive)
    (delete-trailing-whitespace)
    (save-buffer)
)

; C/C++ Mode Handling
(defun mara-c-hook ()
    ; Set style
    (c-add-style "Marastyle" mara-c-style t)

    ; No hungry backspace
    (c-toggle-auto-hungry-state -1)

    ; Show whitespace
    (whitespace-mode 1)

    ; Show line numbers
    (display-line-numbers-mode 1)

    ; Set Save to our custom function
    (define-key c++-mode-map "\C-s" 'mara-c-save)
)

(add-hook 'c-mode-common-hook 'mara-c-hook)

; **************************************************************************************************

(add-to-list 'default-frame-alist '(font . "JetBrains Mono Regular-11.5"))
(set-face-attribute 'default t :font "JetBrains Mono Regular-11.5")

; Theme stuff
(add-to-list 'custom-theme-load-path "~/.emacs.d/themes")
; (load-theme 'zenburn t)
(load-theme 'moe-dark t)

; Cursor settings
; Leave this at the very end of the file please!
; (setq-default cursor-type 'bar)
(set-cursor-color "#de2f63")