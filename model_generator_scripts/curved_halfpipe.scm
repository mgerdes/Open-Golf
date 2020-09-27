(define (generate N inner_radius width height texture_dx texture_length
                  ground_friction ground_cor ground_vel_scale
                  wall_friction wall_cor wall_vel_scale)
  (define-macro (while test . body) 
                `(call-with-exit
                   (lambda (break) 
                     (let continue ()
                       (if (let () ,test)
                           (begin 
                             (let () ,@body)
                             (continue))
                           (break))))))

  (define (bezier p0 p1 p2 p3 t)
    (define p 0)
    (define t1 (- 1 t))
    (set! p (+ p (* p0 t1 t1 t1)))
    (set! p (+ p (* p1 3 t t1 t1)))
    (set! p (+ p (* p2 3 t t t1)))
    (set! p (+ p (* p3 t t t)))
    p
    )

  (define (distance p0 p1)
    (define dx (- (car p0) (car p1)))
    (define dy (- (cadr p0) (cadr p1)))
    (define dz (- (caddr p0) (caddr p1)))
    (sqrt (+ (* dx dx) (* dy dy) (* dz dz))))

  (define points_idx 0)
  (define points (make-vector (round (+ (* N (+ N 6))))))
  (define (add_point x y z)
    (terrain_model_add_point x y z)
    (vector-set! points points_idx (list x y z))
    (set! points_idx (+ points_idx 1))
    )

  (define inner_length (* 0.5 inner_radius pi))
  (define outer_length (* 0.5 (+ inner_radius width) pi))

  (define i 0)
  (while (< i N)
         (define t0 (/ i (- N 1)))
         (define theta0 (* 0.5 pi t0))

         (define j 0)
         (while (< j N)
                (define t1 (/ j (- N 1)))
                (define theta1 (* 0.65 pi t1))

                (define a (* t0 2))
                (if (> a 1) (set! a (- 2 a)) '())
                (set! a (- 1 a))
                (set! a (- 1 (cos (* 0.5 pi a))))

                (define r0 (+ inner_radius (* width (sin theta1))))
                (define x0 (* r0 (cos theta0)))
                (define y0 (* (- 1 a) (* height (* 2 (- 1 (cos theta1))))))
                (define z0 (* r0 (sin theta0)))

                (define r1 (+ inner_radius (* width t1)))
                (define x1 (* r1 (cos theta0)))
                (define y1 0)
                (define z1 (* r1 (sin theta0)))

                (define r (+ r0 (* (- r1 r0) a)))
                (define x (+ x0 (* (- x1 x0) a)))
                (define y (+ y0 (* (- y1 y0) a)))
                (define z (+ z0 (* (- z1 z0) a)))

                (if (= j 0)
                    (begin
                      (define border_radius (- r1 0.5))
                      (define border_x (* border_radius (cos theta0)))
                      (define border_z (* border_radius (sin theta0)))
                      (add_point border_x -3 border_z)
                      (add_point border_x (+ y0 0.25) border_z)
                      (add_point x (+ y0 0.25) z)
                      )
                    '()
                    )

                (add_point x y z)

                (if (= j (- N 1))
                    (begin
                      (define border_radius (+ r 0.5))
                      (define border_x (* border_radius (cos theta0)))
                      (define border_z (* border_radius (sin theta0)))
                      (add_point x (+ y 0.25) z)
                      (add_point border_x (+ y 0.25) border_z)
                      (add_point border_x -3 border_z)
                      )
                    '()
                    )

                (set! j (+ j 1))
                )

         (set! i (+ i 1))
         )

  (define l_vector (make-vector (round N)))
  (set! i 0)
  (while (< i N)
         (vector-set! l_vector (round i) 0)
         (set! i (+ i 1))
         )

  (set! i 0)
  (while (< i (- N 1))
         ;; Border 1
         (begin 
           (define a (+ (* (+ i 0) (+ 6 N)) 0))
           (define b (+ (* (+ i 1) (+ 6 N)) 0))
           (define c (+ (* (+ i 1) (+ 6 N)) 1))
           (define d (+ (* (+ i 0) (+ 6 N)) 1))
           (define tc0x 0)
           (define tc1x 3.25)
           (define tc0y (* (/ i N) inner_length))
           (define tc1y (* (/ (+ i 1) N) inner_length))
           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc1x tc1y
                                   tc0x tc1y
                                   tc0x tc0y
                                   0.25 
                                   wall_cor wall_friction wall_vel_scale 0)
           )

         ;; Border 2
         (begin 
           (define a (+ (* (+ i 0) (+ 6 N)) 1))
           (define b (+ (* (+ i 1) (+ 6 N)) 1))
           (define c (+ (* (+ i 1) (+ 6 N)) 2))
           (define d (+ (* (+ i 0) (+ 6 N)) 2))
           (define tc0x 0)
           (define tc1x 1)
           (define tc0y (* (/ i N) inner_length))
           (define tc1y (* (/ (+ i 1) N) inner_length))
           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc0x tc0y
                                   tc0x tc1y
                                   tc1x tc1y
                                   tc1x tc0y
                                   0.25 
                                   ground_cor ground_friction ground_vel_scale 0)
           )

         ;; Border 3
         (begin 
           (define a (+ (* (+ i 0) (+ 6 N)) 2))
           (define b (+ (* (+ i 1) (+ 6 N)) 2))
           (define c (+ (* (+ i 1) (+ 6 N)) 3))
           (define d (+ (* (+ i 0) (+ 6 N)) 3))
           (define tc0x 0)
           (define tc1x 1)
           (define tc0y (* (/ i N) inner_length))
           (define tc1y (* (/ (+ i 1) N) inner_length))
           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc0x tc0y
                                   tc0x tc1y
                                   tc1x tc1y
                                   tc1x tc0y
                                   0.25 
                                   wall_cor wall_friction wall_vel_scale 0)
           )

         ;; Border 4
         (begin 
           (define a (+ (* (+ i 0) (+ 6 N)) N 2))
           (define b (+ (* (+ i 1) (+ 6 N)) N 2))
           (define c (+ (* (+ i 1) (+ 6 N)) N 3))
           (define d (+ (* (+ i 0) (+ 6 N)) N 3))
           (define tc0x 0)
           (define tc1x 1)
           (define tc0y (* (/ i N) outer_length))
           (define tc1y (* (/ (+ i 1) N) outer_length))
           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc1x tc1y
                                   tc0x tc1y
                                   tc0x tc0y
                                   0.25 
                                   wall_cor wall_friction wall_vel_scale 0)
           )

         ;; Border 5
         (begin 
           (define a (+ (* (+ i 0) (+ 6 N)) N 3))
           (define b (+ (* (+ i 1) (+ 6 N)) N 3))
           (define c (+ (* (+ i 1) (+ 6 N)) N 4))
           (define d (+ (* (+ i 0) (+ 6 N)) N 4))
           (define tc0x 0)
           (define tc1x 1)
           (define tc0y (* (/ i N) outer_length))
           (define tc1y (* (/ (+ i 1) N) outer_length))
           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc1x tc1y
                                   tc0x tc1y
                                   tc0x tc0y
                                   0.25
                                   ground_cor ground_friction ground_vel_scale 0)
           )

         ;; Border 6
         (begin 
           (define a (+ (* (+ i 0) (+ 6 N)) N 4))
           (define b (+ (* (+ i 1) (+ 6 N)) N 4))
           (define c (+ (* (+ i 1) (+ 6 N)) N 5))
           (define d (+ (* (+ i 0) (+ 6 N)) N 5))

           (define ay (cadr (vector-ref points (round a))))
           (define by (cadr (vector-ref points (round b))))
           (define cy (cadr (vector-ref points (round c))))
           (define dy (cadr (vector-ref points (round d))))

           (define tc0y (* (/ i N) outer_length))
           (define tc1y (* (/ (+ i 1) N) outer_length))
           (terrain_model_add_face 4 1 0
                                   a b c d
                                   0 tc0y
                                   0 tc1y
                                   (- by cy) tc1y
                                   (- ay dy) tc0y
                                   0.25
                                   wall_cor wall_friction wall_vel_scale 0)
           )

         (define j 0)
         (define w0 0)
         (define w1 0)
         (while (< j (- N 1))
                (define a (+ (* (+ i 0) (+ N 6)) (+ j 0) 3))
                (define b (+ (* (+ i 1) (+ N 6)) (+ j 0) 3))
                (define c (+ (* (+ i 1) (+ N 6)) (+ j 1) 3))
                (define d (+ (* (+ i 0) (+ N 6)) (+ j 1) 3))

                (define pa (vector-ref points (round a)))
                (define pb (vector-ref points (round b)))
                (define pc (vector-ref points (round c)))
                (define pd (vector-ref points (round d)))

                (define w_dist0 (distance pa pd))
                (define w_dist1 (distance pb pc))

                (define tj0 (/ (+ j 0) (- N 1)))
                (define tj1 (/ (+ j 1) (- N 1)))
                (define ti0 (/ (+ i 0) (- N 1)))
                (define ti1 (/ (+ i 1) (- N 1)))

                (define atcx (+ texture_dx w0))
                (define btcx (+ texture_dx w1))
                (define ctcx (+ texture_dx (+ w1 w_dist1)))
                (define dtcx (+ texture_dx (+ w0 w_dist0)))

                (set! w0 (+ w0 w_dist0))
                (set! w1 (+ w1 w_dist1))

                (define atcy (* texture_length ti0))
                (define btcy (* texture_length ti1))
                (define ctcy (* texture_length ti1))
                (define dtcy (* texture_length ti0))

                (terrain_model_add_face 4 0 1
                                        a b c d
                                        atcx atcy 
                                        btcx btcy 
                                        ctcx ctcy 
                                        dtcx dtcy
                                        0.5
                                        ground_cor ground_friction ground_vel_scale 0)

                (set! j (+ j 1))
                )

         (set! i (+ i 1))
         )
  )
