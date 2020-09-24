(define (generate N width radius a texture_height 
                  border_width border_ground_y)
  (define-macro (while test . body) 
                `(call-with-exit
                   (lambda (break) 
                     (let continue ()
                       (if (let () ,test)
                           (begin 
                             (let () ,@body)
                             (continue))
                           (break))))))

  (define (distance p0 p1)
    (define dx (- (car p0) (car p1)))
    (define dy (- (cadr p0) (cadr p1)))
    (define dz (- (caddr p0) (caddr p1)))
    (sqrt (+ (* dx dx) (* dy dy) (* dz dz))))

  (define points_idx 0)
  (define points (make-vector (round (* 4 N))))
  (define (add_point x y z)
    (terrain_model_add_point x y z)
    (vector-set! points points_idx (list x y z))
    (set! points_idx (+ points_idx 1))
    )

  (define i 0)
  (while (< i N)
         (define t (/ i (- N 1)))
         (define theta (- (* 1.5 pi) (* a 2.0 pi t)))

         (begin
           (define x (* radius (cos theta)))
           (define y (* radius (sin theta)))
           (define z 0)
           (add_point x y z)
           )

         (begin
           (define x (* radius (cos theta)))
           (define y (* radius (sin theta)))
           (define z width)
           (add_point x y z)
           )

         (begin
           (define x (- 0 radius border_width))
           (define y (* radius (sin theta)))
           (define z 0)
           (add_point x y z)
           )

         (begin
           (define x (- 0 radius border_width))
           (define y (* radius (sin theta)))
           (define z width)
           (add_point x y z)
           )

         (set! i (+ i 1))
         )

  (set! i 0)
  (while (< i (- N 1))
         (define t0 (/ i (- N 1)))
         (define t1 (/ (+ i 1) (- N 1)))

         ;; Ground
         (begin 
           (define a (+ (* 4 i) 0))
           (define b (+ (* 4 i) 4))
           (define c (+ (* 4 i) 5))
           (define d (+ (* 4 i) 1))

           (define pa (vector-ref points (round a)))
           (define pb (vector-ref points (round b)))
           (define pc (vector-ref points (round c)))
           (define pd (vector-ref points (round d)))

           (define atcx 0)
           (define btcx 0)
           (define ctcx width)
           (define dtcx width)

           (define atcy (* t0 texture_height))
           (define btcy (* t1 texture_height))
           (define ctcy (* t1 texture_height))
           (define dtcy (* t0 texture_height))

           (terrain_model_add_face 4 0 1
                                   a b c d
                                   atcx atcy 
                                   btcx btcy 
                                   ctcx ctcy 
                                   dtcx dtcy
                                   .5 .4 .3 1 0))

         ;; Side 1
         (begin 
           (define a (+ (* 4 i) 0))
           (define b (+ (* 4 i) 2))
           (define c (+ (* 4 i) 6))
           (define d (+ (* 4 i) 4))

           (define pa (vector-ref points (round a)))
           (define pb (vector-ref points (round b)))
           (define pc (vector-ref points (round c)))
           (define pd (vector-ref points (round d)))

           (define pax (car pa))
           (define pay (car pa))
           (define pbx (car pb))
           (define pby (car pb))
           (define pcx (car pc))
           (define pcy (car pc))
           (define pdx (car pd))
           (define pdy (car pd))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   pay (- pax pbx) 
                                   pby 0.0 
                                   pcy 0.0 
                                   pdy (- pdx pcx) 
                                   0.25 .4 .3 1 0))

         ;; Side 2
         (begin 
           (define a (+ (* 4 i) 3))
           (define b (+ (* 4 i) 1))
           (define c (+ (* 4 i) 5))
           (define d (+ (* 4 i) 7))

           (define pa (vector-ref points (round a)))
           (define pb (vector-ref points (round b)))
           (define pc (vector-ref points (round c)))
           (define pd (vector-ref points (round d)))

           (define pax (car pa))
           (define pay (car pa))
           (define pbx (car pb))
           (define pby (car pb))
           (define pcx (car pc))
           (define pcy (car pc))
           (define pdx (car pd))
           (define pdy (car pd))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   pay 0.0 
                                   pby (- pax pbx) 
                                   pcy (- pdx pcx) 
                                   pdy 0.0
                                   .25 .4 .3 1 0))


         (set! i (+ i 1))
         )

  ;; Back
  (begin 
    (define a 2)
    (define b 3)
    (define c (- (* 4 N) 1))
    (define d (- (* 4 N) 2))

    (define pa (vector-ref points (round a)))
    (define pb (vector-ref points (round b)))
    (define pc (vector-ref points (round c)))
    (define pd (vector-ref points (round d)))

    (define atcx 0)
    (define btcx 0)
    (define ctcx 0)
    (define dtcx 0)

    (define atcy 0)
    (define btcy 0)
    (define ctcy 0)
    (define dtcy 0)

    (terrain_model_add_face 4 1 0
                            a b c d
                            atcx atcy 
                            btcx btcy 
                            ctcx ctcy 
                            dtcx dtcy
                            .5 .4 .3 1 1))

  (begin 
    (define a (- (* 4 N) 1))
    (define b (- (* 4 N) 3))
    (define c (- (* 4 N) 4))
    (define d (- (* 4 N) 2))

    (define pa (vector-ref points (round a)))
    (define pb (vector-ref points (round b)))
    (define pc (vector-ref points (round c)))
    (define pd (vector-ref points (round d)))

    (define atcx 0)
    (define btcx 0)
    (define ctcx 0)
    (define dtcx 0)

    (define atcy 0)
    (define btcy 0)
    (define ctcy 0)
    (define dtcy 0)

    (terrain_model_add_face 4 1 0
                            a b c d
                            atcx atcy 
                            btcx btcy 
                            ctcx ctcy 
                            dtcx dtcy
                            .5 .4 .3 1 1))


  )
