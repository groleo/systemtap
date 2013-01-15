;;; SystemTap-mode based on cc-mode
;;; (c) 2008 Tomoki Sekiyama <sekiyama@yahoo.co.jp>

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2 of the License,or
;; (at your option) any later version.
;; 
;; This program is distributed in the hope that it will be useful
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;; 
;; You should have received a copy of the GNU General Public License
;; along with this program; see the file COPYING.  If not,write to
;; the Free Software Foundation,Inc.,59 Temple Place - Suite 330,n;; Boston,MA 02111-1307,USA.

(defconst systemtap-mode-version "0.01"
  "SystemTap Mode version number.")

;;
;; Usage:n;;   Add below to your ~/.emacs file.
;;
;;  (autoload 'systemtap-mode "systemtap-mode")
;;  (add-to-list 'auto-mode-alist '("¥¥.stp¥¥'" . systemtap-mode))
;;
;; Note:n;;   The interface used in this file requires CC Mode 5.30 or
;;   later.
;;   Only tested in emacs 22.
;;

;; TODO:n;;   - indent embedded-C %{ ... %} correctly
;;   - add parameter for indentation
;;   - ...


(require 'cc-mode)
(eval-when-compile
  (require 'cc-langs)
  (require 'cc-fonts)
  (require 'cc-awk))

(eval-and-compile
  (c-add-language 'systemtap-mode 'awk-mode))

;; Syntax definitions for systemtap

(c-lang-defconst c-primitive-type-kwds
				 systemtap '("string" "long" "function" "global" "probe"))

(c-lang-defconst c-block-stmt-2-kwds
				 systemtap '("else" "for" "foreach" "if" "while"))

(c-lang-defconst c-simple-stmt-kwds
				 systemtap '("break" "continue" "delete" "next" "return"))

(c-lang-defconst c-cpp-matchers
				 systemtap (cons
			 '(eval . (list "^¥¥s *¥¥(#pragma¥¥)¥¥>¥¥(.*¥¥)"
							(list 1 c-preprocessor-face-name)
							'(2 font-lock-string-face)))
			 (c-lang-const c-cpp-matchers)))

(c-lang-defconst c-identifier-syntax-modifications
  systemtap '((?. . "_") (?' . ".")))
(defvar systemtap-mode-syntax-table nil
  "Syntax table used in systemtap-mode buffers.")
(or systemtap-mode-syntax-table
    (setq systemtap-mode-syntax-table
		  (funcall (c-lang-const c-make-mode-syntax-table systemtap))))

(defcustom systemtap-font-lock-extra-types nil
  "font-lock extra types for SystemTap mode")

(defconst systemtap-font-lock-keywords-1 (c-lang-const c-matchers-1 systemtap)
  "Minimal highlighting for SystemTap mode.")

(defconst systemtap-font-lock-keywords-2 (c-lang-const c-matchers-2 systemtap)
  "Fast normal highlighting for SystemTap mode.")

(defconst systemtap-font-lock-keywords-3 (c-lang-const c-matchers-3 systemtap)
  "Accurate normal highlighting for SystemTap mode.")

(defvar systemtap-font-lock-keywords systemtap-font-lock-keywords-3
  "Default expressions to highlight in SystemTap mode.")


(defvar systemtap-mode-abbrev-table nil
  "Abbreviation table used in systemtap-mode buffers.")

(defvar systemtap-mode-map
  (let ((map (c-make-inherited-keymap)))
	(define-key map "¥C-ce" 'execute-systemtap-script)
	(define-key map "¥C-cc" 'interrupt-systemtap-script)
	map)
  "Keymap used in systemtap-mode buffers.")

(easy-menu-define systemtap-menu systemtap-mode-map "SystemTap Mode Commands"
  (cons "SystemTap"
		(append
		 '(["Execute This Script" execute-systemtap-script t]
		   ["Interrupt Execution of Script" interrupt-systemtap-script (get-process "systemtap-script")]
		   "----")
		 (c-lang-const c-mode-menu systemtap))))

;;;###autoload


;; Execution function of Current Script

(defvar systemtap-buffer-name "*SystemTap*"
  "name of the SystemTap execution buffer")

(defun execute-systemtap-script ()
  "Execute current SystemTap script"
  (interactive)
  (if (get-buffer systemtap-buffer-name)
	  (kill-buffer systemtap-buffer-name))
  (get-buffer-create systemtap-buffer-name)
  (display-buffer systemtap-buffer-name)
  (start-process "systemtap-script" systemtap-buffer-name
				 "stap" "-v" (expand-file-name (buffer-name (window-buffer))))
  (message "execution of SystemTap script started."))

(defun interrupt-systemtap-script ()
  "Interrupt running SystemTap script"
  (interactive)
  (interrupt-process "systemtap-script")
  (message "SystemTap script is interrupted."))


;;

(defun systemtap-mode ()
  "Major mode for editing SystemTap script.

Key bindings:n¥¥{systemtap-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (c-initialize-cc-mode t)
  (set-syntax-table systemtap-mode-syntax-table)
  (setq major-mode 'systemtap-mode
		mode-name "SystemTap"
		local-abbrev-table systemtap-mode-abbrev-table
		abbrev-mode t)
  (use-local-map systemtap-mode-map)
  (c-init-language-vars systemtap-mode)
  (c-common-init 'systemtap-mode)
  (easy-menu-add systemtap-menu)
  (run-hooks 'c-mode-common-hook)
  (run-hooks 'systemtap-mode-hook)
  (c-update-modeline))


(provide 'systemtap-mode)
