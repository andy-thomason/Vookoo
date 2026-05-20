  Why not K = JACOBI_ITERS in one dispatch?                                                                                                 
   
  Two hard limits:                                                                                                                          
                                                            
  1. Shared memory size. For K inner iterations you need a halo of K cells on each side, so the tile is (8+2K)³. With K=40:                 
  
  tile_memory = (WG + 2×K)³ × sizeof(float)
  (8 + 2*40)³ × 4 bytes = 88³ × 4 = 2.7 MB per workgroup                                                                                      
                                                                                                                                            
  The shared memory limit is 48KB. K=40 is 56× over budget. The largest K that fits with float is 6: (8+2*6)³ × 4 = 32KB.                    
                                                                                                                                            
  2. Information propagation. Even with unlimited shared memory it wouldn't produce correct results. The halo is loaded once from global    
  memory at the start of each outer dispatch and stays frozen. Pressure information can only travel K cells within each workgroup per outer
  dispatch. Adjacent workgroups don't see each other's updates until the next outer dispatch writes back to global memory.                  
                                                            
  So after one outer dispatch with K=40, pressure information has only spread 40 cells within each isolated 8³ workgroup domain — but the   
  neighbouring workgroups know nothing about it. Cells near the boundary of the 8³ region used stale halo data for iterations 2–40. No cells
   get the benefit of globally-consistent 40-iteration convergence.                                                                         
                                                            
  After N outer dispatches, global propagation distance is N × K cells. The product is what determines convergence quality — not K alone.   
  So:
                                                                                                                                            
  ┌─────┬──────────────────────┬─────┬──────────┐                                                                                           
  │  K  │ N (outer dispatches) │ N×K │ Barriers │
  ├─────┼──────────────────────┼─────┼──────────┤                                                                                           
  │ 1   │ 40                   │ 40  │ 40       │           
  ├─────┼──────────────────────┼─────┼──────────┤
  │ 4   │ 10                   │ 40  │ 10       │                                                                                           
  ├─────┼──────────────────────┼─────┼──────────┤
  │ 6   │ 7                    │ 42  │ 7        │                                                                                           
  └─────┴──────────────────────┴─────┴──────────┘           

  K=4 and K=6 give roughly the same global propagation as K=1 with 40 outer dispatches, but with 4–6× fewer barriers. The sweet spot is the 
  largest K that fits in shared memory, which is K=4 with float or K=6 pushing against the 48KB wall. Going beyond that doesn't improve
  solution quality — it just wastes inner iterations on stale-halo data.
