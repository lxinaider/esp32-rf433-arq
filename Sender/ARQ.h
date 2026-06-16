#ifndef ARQ_H
#define ARQ_H

#define PREAMBLE_BYTE      0x2A
#define PREAMBLE_BYTES_TX  12    // preâmbulo longo para frames DATA/END
#define PREAMBLE_BYTES_ACK 12    // mesmo comprimento para ACKs (canal é igual)
#define TIMEOUT_MS         3000  // timeout aguardando ACK ou frame
#define RX_SETTLE_MS       20    // aguarda o receptor estabilizar após TX desligar

#endif

