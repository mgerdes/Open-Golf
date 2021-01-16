(define fib (lambda (n) ; test comment
  (if (== n 0)
    0
    (if (== n 1)
      1
      (+ (fib (- n 1)) (fib (- n 2)))))))

(= a (array))

(for (= i 0) (< i 10) (++ i)
    (array-push a 10))

(define map (lambda (f a)
  (define a1 (array-create))
  (for (= i 0) (< i (array-length a)) (++ i)
    (array-push a1 (f array-get a i)))
  a1))

(define a (array))
(for (= i 0) (< i 10) (++ i)
  (array-push a i))

(map fib a)
