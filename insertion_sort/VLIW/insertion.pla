// === FASE 1: Cargar parámetros ===
LW R1 0(R0)     // n
LW R2 1(R0)     // src
LW R3 2(R0)     // dest

// === FASE 2: Copiar origen a destino (flotantes) ===
ADD R4 R2 R0
ADD R5 R3 R0
ADD R6 R2 R1

COPY:
    LF F1 0(R4)
    SF F1 0(R5)
    ADDI R4 R4 #1
    ADDI R5 R5 #1
    BNE R4 R6 COPY

// === FASE 3: Insertion Sort para flotantes ===
ADDI R10 R0 #1

ADD R8 R3 R10       // i = base + 1
ADD R9 R3 R1        // end_exclusive = base + n

OUTER:
    BEQ R8 R9 END

    LF F2 0(R8)         // key = A[i]
    ADD R12 R8 R0       // j = i

INNER:
    BEQ R12 R3 INSERT

    ADDI R13 R12 #-1
    LF F3 0(R13)        // A[j-1]

    // si A[j-1] > key, desplazar
    BGTF F3 F2 SHIFT
    BEQ R0 R0 INSERT

SHIFT:
    SF F3 0(R12)        // A[j] = A[j-1]
    ADD R12 R13 R0      // j = j - 1
    BEQ R0 R0 INNER

INSERT:
    SF F2 0(R12)        // A[j] = key
    ADDI R8 R8 #1
    BEQ R0 R0 OUTER

END: