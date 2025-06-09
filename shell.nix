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
        
        export DB_USER="dbuser"
        export DB_PASSWORD="dbpassword"
        export DB_NAME="tanstarter"
        export VITE_BASE_URL=http://localhost:3000
        export DATABASE_URL="postgresql://$DB_USER:$DB_PASSWORD@localhost:5432/$DB_NAME"
        # Only for testing! not production!
        export BETTER_AUTH_SECRET="23a4e0160ee4a0d5cf2790839c5a166a7e7f647c9fe756c6fbd4765a67c9d079"

        # Install node.js packages using pnpm
        pnpm install --prefer-offline 

        # Start PostgreSQL service
        export PGDATA=$PWD/pgdata

        if [ ! -d "$PGDATA" ]; then
            echo "Initializing PostgreSQL database at $PGDATA"
            mkdir -p $PGDATA
            initdb -D $PGDATA
            export NEW_DB=1
        fi

        # Need to kill the PostgreSQL server when exiting the shell
        trap \
            "
                pg_ctl stop -D $PGDATA -o '-k $PWD'
            " EXIT
        
        # Start PostgreSQL server
        pg_ctl start -D $PGDATA -o "-k '$PWD'" -l db.log

        if [ $NEW_DB ]; then
            # Its initial setup time!

            # Create DB user
            psql -h $PWD postgres -c "CREATE USER $DB_USER WITH PASSWORD '$DB_PASSWORD';"
            
            # Create database
            psql -h $PWD postgres -c "CREATE DATABASE $DB_NAME WITH OWNER $DB_USER;"
            
            # Push BetterAuth tables
            pnpm db push
        fi

        # Run the 
        pnpm dev 
    '';
}