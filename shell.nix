with import <nixpkgs> { config.allowUnfree = true; };

stdenv.mkDerivation {
    name = "node";

    buildInputs = with pkgs; [
        # Node.js and npm
        nodejs
        nodePackages.pnpm

        # Database
        (pkgs.postgresql.withPackages (p: [ p.timescaledb ]))

        (vscode-with-extensions.override {
            vscodeExtensions = with vscode-extensions; [
                # Recommended extensions for the workspace
                bradlc.vscode-tailwindcss
                #dbaeumer.vscode
                esbenp.prettier-vscode
                # Nicer Nix syntax highlighting
                bbenoist.nix
            ];
        })
    ];


    shellHook = ''
        export PATH="$PWD/node_modules/.bin/:$PATH"

        # Install node.js packages using pnpm
        pnpm install --prefer-offline 

        # Start PostgreSQL service
        export PGDATA=$PWD/pgdata
        if [ ! -d "$PGDATA" ]; then
            echo "Initializing PostgreSQL database at $PGDATA"
            mkdir -p $PGDATA
            initdb -D $PGDATA
        fi

        # Need to kill the PostgreSQL server when exiting the shell
        trap \
            "
                pg_ctl stop -D $PGDATA -o '-k $PWD'
            " EXIT
        
        # Start PostgreSQL server
        pg_ctl start -D $PGDATA -o "-k '$PWD'" -l db.log
    '';
}