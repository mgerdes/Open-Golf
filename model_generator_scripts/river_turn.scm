(define (generate N inner_radius width tc_dy)
  (define-macro (while test . body) 
                `(call-with-exit
                   (lambda (break) 
                     (let continue ()
                       (if (let () ,test)
                           (begin 
                             (let () ,@body)
                             (continue))
                           (break))))))

  (define points (make-vector (round (* 2 N))))
  (define points_idx 0)

  (define (add_point x y z)
    (terrain_model_add_point x y z)
    (vector-set! points points_idx (list x y z))
    (set! points_idx (+ points_idx 1))
    )

  (define i 0)
  (while (< i N)
	 (define theta (* 0.5 pi (/ i (- N 1))))

	 (define x0 (* inner_radius (cos theta)))
	 (define y0 0)
	 (define z0 (* inner_radius (sin theta)))
	 (add_point x0 y0 z0)

	 (define x0 (* (+ inner_radius width) (cos theta)))
	 (define y0 0)
	 (define z0 (* (+ inner_radius width) (sin theta)))
	 (add_point x0 y0 z0)

	 (set! i (+ 1 i))
	 )

  (set! i 0)
  (while (< i (- N 1))
	 (define a (round (+ (* 2 i) 2)))
	 (define b (round (+ (* 2 i) 3)))
	 (define c (round (+ (* 2 i) 1)))
	 (define d (round (+ (* 2 i) 0)))

	 (define t0 (/ i (- N 1)))
	 (define t1 (/ (+ i 1) (- N 1)))

	 (define atcx 0)
	 (define atcy (+ t1 tc_dy))
	 (define btcx 1)
	 (define btcy (+ t1 tc_dy))
	 (define ctcx 1)
	 (define ctcy (+ t0 tc_dy))
	 (define dtcx 0)
	 (define dtcy (+ t0 tc_dy))

	 (terrain_model_add_face 4 0 0
				 a b c d
				 atcx atcy
				 btcx btcy
				 ctcx ctcy
				 dtcx dtcy
				 1 0 0 0 0)
	 (set! i (+ i 1))
	 )

  )

