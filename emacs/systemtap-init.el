(autoload 'systemtap-mode "systemtap-mode.el")
(setq auto-mode-alist (append '(("\\.stp$" . systemtap-mode)) auto-mode-alist))
(setq auto-mode-alist (append '(("\\.stpm$" . systemtap-mode)) auto-mode-alist))
