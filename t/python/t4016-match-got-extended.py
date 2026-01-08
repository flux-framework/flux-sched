input()  # Discard: Expiration of allocation
input()  # Discard: Expiration of first reservation
reservation_exp = int(input())  # Expiration of reservation
match_wo_alloc_exp = int(input())  # Expiration of match_wo_alloc
# Does the match_wo_alloc expire at the start of the reservation?
exit(reservation_exp - match_wo_alloc_exp != 300)
